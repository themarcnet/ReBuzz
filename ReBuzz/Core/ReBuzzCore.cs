﻿using Buzz.MachineInterface;
using BuzzGUI.Common;
using BuzzGUI.Common.InterfaceExtensions;
using BuzzGUI.Interfaces;
using BuzzGUI.MachineView;
using BuzzGUI.MachineView.HDRecorder;
using BuzzGUI.PianoKeyboard;
using libsndfile;
using Microsoft.Win32;
using NAudio.Midi;
using ReBuzz.Audio;
using ReBuzz.Common;
using ReBuzz.Core.Actions.GraphActions;
using ReBuzz.FileOps;
using ReBuzz.MachineManagement;
using ReBuzz.Midi;
using ReBuzz.Properties;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Configuration;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;
using Timer = System.Timers.Timer;

namespace ReBuzz.Core
{
    [StructLayout(LayoutKind.Sequential)]
    public struct BuzzGlobalState
    {
        public int AudioFrame;
        public int ADWritePos;
        public int ADPlayPos;
        public int SongPosition;
        public int LoopStart;
        public int LoopEnd;
        public int SongEnd;
        public int StateFlags;
        public byte MIDIFiltering;
        public byte SongClosing;
    };

    public class ReBuzzCore : IBuzz, INotifyPropertyChanged
    {
        public static int buildNumber = int.Parse(Resources.BuildNumber);
        public static MasterInfoExtended masterInfo;
        public static SubTickInfoExtended subTickInfo;
        public static readonly int SubTicsPerTick = 8;
        internal static BuzzGlobalState GlobalState;
        public static string AppDataPath = "ReBuzz";
        readonly DebugWindow debugWindow;
        private readonly ConcurrentStreamWriter DebugStreamWrite;

        public readonly bool AUTO_CONVERT_WAVES = false;

        public int HostVersion { get => 66; } // MI_VERSION

        public static readonly int SF_PLAYING = 1;
        public static readonly int SF_RECORDING = 2;

        SongCore songCore;
        public ISong Song { get => songCore; }

        public SongCore SongCore
        {
            get => songCore;
            set
            {
                songCore = value;
                MidiControllerAssignments.Song = songCore;
            }
        }

        BuzzView activeView = BuzzView.MachineView;
        public ReBuzzTheme Theme { get; set; }

        public System.Drawing.Color GetThemeColour(string name)
        {
            var wpfColour = ThemeColors[name];

            //Convert to System.Drawing.Color
            return System.Drawing.Color.FromArgb(wpfColour.A, wpfColour.R, wpfColour.G, wpfColour.B);
        }

        public BuzzView ActiveView { get { return activeView; } set { activeView = value; PropertyChanged.Raise(this, "ActiveView"); } }
        double masterVolume = 1;
        public double MasterVolume { get => masterVolume; set { masterVolume = value; PropertyChanged.Raise(this, "MasterVolume"); SetModifiedFlag(); } }

        public Tuple<double, double> VUMeterLevel { get; set; }

        internal MidiControllerAssignments MidiControllerAssignments { get; set; }

        internal DispatcherTimer dtEngineThread;

        int speed;
        bool playing;
        bool recording;
        bool looping;
        bool audioDeviceDisabled;

        int bpm = 126;
        public int BPM
        {
            get => bpm; set
            {
                if (value >= 16 && value <= 500 && bpm != value)
                {
                    bpm = value;
                    if (songCore != null)
                    {
                        var master = songCore.MachinesList.FirstOrDefault(m => m.DLL.Info.Type == MachineType.Master);
                        master.ParameterGroups[1].Parameters[1].SetValue(0, value);
                    }
                    // Actual BPM is changed in WorkManager
                    Application.Current.Dispatcher.BeginInvoke(() =>
                    {
                        try
                        {
                            if (PropertyChanged != null)
                                PropertyChanged?.Raise(this, "BPM");
                            SetModifiedFlag();
                        }
                        catch (Exception ex)
                        {
                            DCWriteLine(ex.Message);
                        }
                    });
                }
            }
        }

        int tpb = 4;
        public int TPB
        {
            get => tpb; set
            {
                if (value >= 1 && value <= 32 && tpb != value)
                {
                    tpb = value;
                    if (songCore != null)
                    {
                        var master = songCore.MachinesList.FirstOrDefault(m => m.DLL.Info.Type == MachineType.Master);
                        master.ParameterGroups[1].Parameters[2].SetValue(0, value);
                    }
                    // Actual TPB is changed in WorkManager
                    Application.Current.Dispatcher.BeginInvoke(() =>
                    {
                        try
                        {
                            if (PropertyChanged != null)
                                PropertyChanged?.Raise(this, "TPB");
                            SetModifiedFlag();
                        }
                        catch (Exception ex)
                        {
                            DCWriteLine(ex.Message);
                        }
                    });
                }
            }
        }
        public int Speed { get => speed; set { speed = value; PropertyChanged.Raise(this, "Speed"); } }

        internal bool StartPlaying()
        {
            if (preparePlaying)
            {
                preparePlaying = false;
                playing = true;
                GlobalState.StateFlags |= SF_PLAYING;
                Application.Current.Dispatcher.BeginInvoke(() =>
                {
                    PropertyChanged.Raise(this, "Playing");
                });
                return true;
            }
            return false;
        }

        bool preparePlaying = false;

        public bool Playing
        {
            get => playing;
            set
            {
                if (!value)
                {
                    preparePlaying = false;
                    playing = false;
                    GlobalState.StateFlags &= ~SF_PLAYING;
                    StopPlayingWave();

                    foreach (var machine in songCore.MachinesList)
                    {
                        MachineManager.Stop(machine);
                    }

                    if (Recording)
                    {
                        Recording = false;
                    }

                    SoloPattern = null;
                    PropertyChanged.Raise(this, "Playing");
                }

                if (value)
                {
                    // Sync with tick == 0
                    preparePlaying = true;
                }
            }
        }
        public bool Recording
        {
            get => recording;
            set
            {
                recording = value;

                if (recording)
                    ReBuzzCore.GlobalState.StateFlags |= SF_RECORDING;
                else
                    ReBuzzCore.GlobalState.StateFlags &= ~SF_RECORDING;

                if (recording && !playing)
                {
                    Playing = true;
                }
                PropertyChanged.Raise(this, "Recording");
            }
        }
        public bool Looping { get => looping; set { looping = value; PropertyChanged.Raise(this, "Looping"); } }
        public bool AudioDeviceDisabled
        {
            get => audioDeviceDisabled;
            set
            {
                audioDeviceDisabled = value;
                PropertyChanged.Raise(this, "AudioDeviceDisabled");
            }
        }

        private IList<string> midiControllers = new string[] { };

        public ReadOnlyCollection<string> MIDIControllers
        {
            get => new ReadOnlyCollection<string>(midiControllers);
            set
            {
                midiControllers = value;
                PropertyChanged.Raise(this, "MIDIControllers");
            }
        }

        IIndex<string, Color> themeColors;
        public IIndex<string, Color> ThemeColors { get => themeColors; set => themeColors = value; }

        public IMenuItem MachineIndex { get; set; }

        PreferencesWindow preferencesWindow;

        // MIDI
        IMachine midiFocusMachine;
        public IMachine MIDIFocusMachine
        {
            get => midiFocusMachine;
            set
            {
                lock (AudioLock)
                {
                    if (midiFocusMachine != value)
                    {
                        if (midiFocusMachine != null)
                        {
                            MachineManager.LostMidiFocus(midiFocusMachine as MachineCore);
                        }

                        midiFocusMachine = value;

                        if (midiFocusMachine != null)
                        {
                            MachineManager.GotMidiFocus(midiFocusMachine as MachineCore);
                        }

                        PropertyChanged.Raise(this, "MIDIFocusMachine");
                    }
                }
            }
        }

        private bool midiFocusLocked = false;
        public bool MIDIFocusLocked { get => midiFocusLocked; set { midiFocusLocked = value; PropertyChanged.Raise(this, "MIDIFocusLocked"); } }

        private bool midiActivity;

        public bool MIDIActivity
        {
            get => midiActivity;
            set
            {
                midiActivity = value;

                if (midiActivity)
                {
                    Timer midiActivityTimer = new Timer();
                    midiActivityTimer.Interval = 500;
                    midiActivityTimer.AutoReset = false;

                    midiActivityTimer.Elapsed += (sender, e) =>
                    {
                        MIDIActivity = false;
                        PropertyChanged.Raise(this, "MIDIActivity");
                        midiActivityTimer.Stop();

                    };
                    midiActivityTimer.Start();
                }
                PropertyChanged.Raise(this, "MIDIActivity");
            }
        }

        // Misc
        KeyboardWindow keyboardWindow = null;
        bool isPianoKeyboardVisible = false;
        public bool IsPianoKeyboardVisible
        {
            get => isPianoKeyboardVisible;
            set
            {
                isPianoKeyboardVisible = value;

                if (keyboardWindow == null)
                {
                    keyboardWindow = new KeyboardWindow(this);

                    keyboardWindow.Topmost = true;
                    var interop = new WindowInteropHelper(keyboardWindow);
                    interop.Owner = MachineViewHWND;
                }

                if (isPianoKeyboardVisible == true)
                {
                    keyboardWindow.Show();
                }
                else
                {
                    keyboardWindow.Hide();
                }
                PropertyChanged.Raise(this, "IsPianoKeyboardVisible");
            }
        }

        bool isSettingsWindowVisible;
        public bool IsSettingsWindowVisible
        {
            get => isSettingsWindowVisible;
            set
            {
                isSettingsWindowVisible = value;
                if (isSettingsWindowVisible)
                {
                    ShowSettings.Invoke("");
                }
                else
                {
                }
            }
        }

        bool isCPUMonitorWindowVisible;
        public bool IsCPUMonitorWindowVisible { get => isCPUMonitorWindowVisible; set { isCPUMonitorWindowVisible = value; PropertyChanged.Raise(this, "IsCPUMonitorWindowVisible"); } }

        bool isHardDiskRecorderWindowVisible;
        HDRecorderWindow hDRecorderWindow;
        public bool IsHardDiskRecorderWindowVisible
        {
            get => isHardDiskRecorderWindowVisible;
            set
            {
                isHardDiskRecorderWindowVisible = value;

                if (hDRecorderWindow == null)
                {
                    hDRecorderWindow = new HDRecorderWindow();
                    var rd = Utils.GetUserControlXAML<ResourceDictionary>("MachineView\\MachineView.xaml");
                    hDRecorderWindow.Resources.MergedDictionaries.Add(rd);

                    hDRecorderWindow.MachineGraph = SongCore;

                    hDRecorderWindow.Topmost = true;
                    var interop = new WindowInteropHelper(hDRecorderWindow);
                    interop.Owner = MachineViewHWND;

                    hDRecorderWindow.Closed += (sender, e) =>
                    {
                        hDRecorderWindow.MachineGraph = null;
                        hDRecorderWindow = null;
                        isHardDiskRecorderWindowVisible = false;
                        PropertyChanged.Raise(this, "IsHardDiskRecorderWindowVisible");
                    };
                }

                if (isHardDiskRecorderWindowVisible == true)
                {
                    hDRecorderWindow.Show();
                }
                else
                {
                    hDRecorderWindow.Close();
                }
                PropertyChanged.Raise(this, "IsHardDiskRecorderWindowVisible");
            }
        }

        public event Action<bool> FullScreenChanged;
        bool fullScreen;
        public bool IsFullScreen
        {
            get => fullScreen;
            set
            {
                fullScreen = value;
                if (FullScreenChanged != null)
                {
                    FullScreenChanged.Invoke(fullScreen);
                }
            }
        }

        public int BuildNumber { get => int.Parse(Resources.BuildNumber); }
        public string BuildString { get => "ReBuzz Build " + BuildNumber + " " + Resources.BuildDate; }

        Dictionary<string, MachineDLL> machineDLLsList = new Dictionary<string, MachineDLL>();
        internal Dictionary<string, MachineDLL> MachineDLLsList { get => machineDLLsList; set => machineDLLsList = value; }
        public BuzzGUI.Interfaces.ReadOnlyDictionary<string, IMachineDLL> MachineDLLs
        {
            get => new BuzzGUI.Interfaces.ReadOnlyDictionary<string, IMachineDLL>(MachineDLLsList
                .ToDictionary(p => p.Key, p => (IMachineDLL)p.Value));
        }

        List<Instrument> InstrumentList { get; set; }
        public ReadOnlyCollection<IInstrument> Instruments { get => InstrumentList.Cast<IInstrument>().ToReadOnlyCollection(); }

        private List<string> audioDrivers;

        public ReadOnlyCollection<string> AudioDrivers { get => audioDrivers.ToReadOnlyCollection(); }
        public List<string> AudioDriversList { get => audioDrivers; set => audioDrivers = value; }

        private string selectedAudioDriver;
        public string SelectedAudioDriver
        {
            get => selectedAudioDriver;
            set
            {
                if (SelectedAudioDriver != value)
                {
                    try
                    {
                        AudioEngine.CreateAudioOut(value);
                        if (AudioEngine.SelectedOutDevice != null)
                        {
                            selectedAudioDriver = AudioEngine.SelectedOutDevice.Name;
                            RegistryEx.Write("AudioDriver", selectedAudioDriver, "Settings");
                            AudioEngine.Play();
                            PropertyChanged.Raise(this, "SelectedAudioDriver");
                        }
                    }
                    catch (Exception e)
                    {
                        Utils.MessageBox("Selected Audio Driver Error: " + e.Message, "Selected Audio Driver Error");
                    }
                }
            }
        }

        public int SelectedAudioDriverSampleRate
        {
            get => masterInfo.SamplesPerSec;
            set
            {
                lock (ReBuzzCore.AudioLock)
                {
                    masterInfo.SamplesPerSec = value;
                    UpdateMasterInfo();
                    MachineManager.ResetMachines();         // Some machines need this?
                }
                PropertyChanged.Raise(this, "SelectedAudioDriverSampleRate");
            }
        }

        readonly int SubTickSize = 260;
        internal void UpdateMasterInfo()
        {
            if (masterInfo.SamplesPerSec > 0)
            {
                masterInfo.BeatsPerMin = bpm;
                masterInfo.TicksPerBeat = tpb;

                masterInfo.AverageSamplesPerTick = 60.0 * masterInfo.SamplesPerSec / (masterInfo.BeatsPerMin * (double)masterInfo.TicksPerBeat);
                masterInfo.SamplesPerTick = (int)masterInfo.AverageSamplesPerTick;
                masterInfo.TicksPerSec = masterInfo.BeatsPerMin * masterInfo.TicksPerBeat / 60.0f;
                masterInfo.PosInTick = 0;

                int subTickCount = masterInfo.SamplesPerTick / SubTickSize;
                subTickInfo.AverageSamplesPerSubTick = masterInfo.SamplesPerTick / (double)subTickCount;
                subTickInfo.SamplesPerSubTick = (int)(subTickInfo.AverageSamplesPerSubTick);
                subTickInfo.SubTicksPerTick = subTickCount;
                subTickInfo.CurrentSubTick = 0;
                subTickInfo.PosInSubTick = 0;

                subTickInfo.SubTickReminderCounter = 0;// masterInfo.SamplesPerTick % subTickInfo.SubTicksPerTick;
            }
        }

        bool overrideAudioDriver = false;

        public bool OverrideAudioDriver
        {
            get => overrideAudioDriver;
            set
            {
                // Don't do anything if driver aldeary overrided
                if (overrideAudioDriver && value == true)
                    return;

                overrideAudioDriver = value;
            }
        }

        public IEditContext EditContext { get; set; }

        long performanceCountTime = 0;
        internal BuzzPerformanceData PerformanceCurrent { get; set; }
        public BuzzPerformanceData PerformanceData { get; set; }

        public IntPtr MachineViewHWND { get; set; }

        public event Action<string> ThemeChanged;
        readonly List<string> themes;
        public ReadOnlyCollection<string> Themes { get => themes.ToReadOnlyCollection(); }

        public string SelectedTheme
        {
            get => RegistryEx.Read<string>("Theme", "<default>", "Settings");
            set
            {
                RegistryEx.Write<string>("Theme", value, "Settings");
                if (ThemeChanged != null)
                {
                    ThemeChanged.Invoke(value);
                }
            }
        }
        public IntPtr MainWindowHandle { get; internal set; }
        public MachineManager MachineManager { get; internal set; }

        MachineDatabase machineDB;
        internal MachineDatabase MachineDB
        {
            get => machineDB;
            set
            {
                machineDB = value;
                MachineIndex = machineDB.IndexMenu;
                UpdateInstrumentList(MachineDB);
            }
        }

        bool modified;
        public bool Modified
        {
            get => modified;
            internal set
            {
                if (masterLoading)
                    return;

                modified = value;
                Application.Current.Dispatcher.BeginInvoke(() =>
                PropertyChanged.Raise(this, "Modified"));
            }
        }
        public IPattern PatternEditorPattern { get; private set; }
        internal PatternCore SoloPattern { get; set; }

        public event Action<IOpenSong> OpenSong;
        public event Action<ISaveSong> SaveSong;
        public event Action<int> MIDIInput;
        public event Action PatternEditorActivated;
        public event Action SequenceEditorActivated;
        public event Action<float[], bool, SongTime> MasterTap;
        public event PropertyChangedEventHandler PropertyChanged;

        public event Action<string> ShowSettings;

        public event Action<BuzzCommand> BuzzCommandRaised;

        internal MidiEngine MidiInOutEngine { get; }

        DispatcherTimer dtVUMeter;

        readonly Timer timerAutomaticBackups;
        public ReBuzzCore()
        {
            // Init process and thread priorities
            ProcessAndThreadProfile.Profile2();

            //DefaultPatternEditor = "Modern Pattern Editor";
            DefaultPatternEditor = "ReBuzzPatternXP";

            Global.GeneralSettings.PropertyChanged += GeneralSettings_PropertyChanged;
            Global.EngineSettings.PropertyChanged += EngineSettings_PropertyChanged;

            masterInfo = new MasterInfoExtended()
            {
                SamplesPerSec = 44100,
                SamplesPerTick = (int)((60 * 44100) / (126 * 4.0)),
                TicksPerSec = (float)(126 * 4 / 60.0)
            };

            subTickInfo = new SubTickInfoExtended()
            {
                CurrentSubTick = 0,
                PosInSubTick = 0
            };

            UpdateMasterInfo();

            GlobalState.LoopStart = 0;
            GlobalState.LoopEnd = 16;
            GlobalState.SongEnd = 16;

            PerformanceData = new BuzzPerformanceData();
            PerformanceCurrent = new BuzzPerformanceData();

            VUMeterLevel = new Tuple<double, double>(0, 0);
            maxSampleLeft = -1;
            maxSampleRight = -1;

            this.Gear = Gear.LoadGearFile(Global.BuzzPath + "\\Gear\\gear_defaults.xml");
            var moreGear = Gear.LoadGearFile(Global.BuzzPath + "\\Gear\\gear.xml");
            Gear.Merge(moreGear);

            this.Theme = ReBuzzTheme.LoadCurrentTheme(this);

            debugWindow = new DebugWindow();
            DebugStreamWrite = debugWindow.DebugStreamWriter;

            DCWriteLine(BuildString);

            MidiInOutEngine = new MidiEngine(this);
            MidiInOutEngine.OpenMidiInDevices();
            MidiInOutEngine.OpenMidiOutDevices();

            MidiControllerAssignments = new MidiControllerAssignments(this);
            MIDIControllers = MidiControllerAssignments.GetMidiControllerNames().ToReadOnlyCollection();

            themes = Utils.GetThemes();

            AppDomain currentDomain = AppDomain.CurrentDomain;
            currentDomain.AssemblyResolve += new ResolveEventHandler(MyResolveEventHandler);

            timerAutomaticBackups = new Timer();
            timerAutomaticBackups.Interval = 10000;
            timerAutomaticBackups.AutoReset = true;
            timerAutomaticBackups.Elapsed += (sender, e) =>
            {
                if (!Playing && SongCore.SongName != null && Modified)
                {
                    try
                    {
                        string backupName = Path.Combine(Path.GetDirectoryName(SongCore.SongName), Path.GetFileNameWithoutExtension(SongCore.SongName) + ".backup");
                        File.Copy(SongCore.SongName, backupName, true);
                    }
                    catch (Exception) { }
                }
            };

            if (Global.GeneralSettings.WPFSoftwareRendering)
            {
                RenderOptions.ProcessRenderMode = RenderMode.SoftwareOnly;
            }

            // These are not good for real time audio. Just use defaults.
            /*
            if (Global.EngineSettings.LowLatencyGC)
            {
                // GCLatencyMode.LowLatency is bad for performance
                // Seems that this has little positive effects on real time audio
                // SustainedLowLatency might be the way to go...
                GCSettings.LatencyMode = GCLatencyMode.SustainedLowLatency;
            }
            */

            Process.GetCurrentProcess().PriorityClass = ProcessAndThreadProfile.ProcessPriorityClassMainProcess;
            Utils.SetProcessorAffinityMask(true);

            dtEngineThread = new DispatcherTimer();
            dtEngineThread.Interval = TimeSpan.FromSeconds(1 / 30.0);
            dtEngineThread.Tick += (s, e) =>
            {
                foreach (var m in Song.Machines)
                {
                    var machine = m as MachineCore;
                    machine.UpdateLastEngineThread();
                }
            };

            if (MachineView.StaticSettings.ShowEngineThreads)
                dtEngineThread.Start();

            MachineView.StaticSettings.PropertyChanged += (s, e) =>
            {
                if (e.PropertyName == "ShowEngineThreads")
                {
                    if (MachineView.StaticSettings.ShowEngineThreads)
                        dtEngineThread.Start();
                    else
                        dtEngineThread.Stop();
                }
            };
        }

        public void StartEvents()
        {
            if (Global.GeneralSettings.AutomaticBackups)
            {
                timerAutomaticBackups.Start();
            }
        }

        private void EngineSettings_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            // These are not good for real time audio. Just use defaults.
            /*
            if (e.PropertyName == "LowLatencyGC")
            {
                if (Global.EngineSettings.LowLatencyGC)
                {
                    GCSettings.LatencyMode = GCLatencyMode.SustainedLowLatency;
                }
                else
                {
                    GCSettings.LatencyMode = GCLatencyMode.Interactive;
                }
            }
            */
        }

        private void DeleteBackup()
        {
            if (SongCore.SongName != null)
            {
                timerAutomaticBackups.Stop();
                int len = SongCore.SongName.Length;
                string backupName = SongCore.SongName.Remove(len - 4, 4) + ".backup";
                if (File.Exists(backupName))
                {
                    File.Delete(backupName);
                }
                timerAutomaticBackups.Start();
            }

        }

        private void GeneralSettings_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == "AutomaticBackups")
            {
                if (Global.GeneralSettings.AutomaticBackups)
                {
                    timerAutomaticBackups.Start();
                }
                else
                {
                    timerAutomaticBackups.Stop();
                }
            }
            else if (e.PropertyName == "WPFSoftwareRendering")
            {
                if (Global.GeneralSettings.WPFSoftwareRendering)
                {
                    RenderOptions.ProcessRenderMode = RenderMode.SoftwareOnly;
                }
                else
                {
                    RenderOptions.ProcessRenderMode = RenderMode.Default;
                }
            }
        }

        private Assembly MyResolveEventHandler(object sender, ResolveEventArgs args)
        {
            var loadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
            var loadedAssembly = AppDomain.CurrentDomain.GetAssemblies().Where(a => a.FullName == args.Name).FirstOrDefault();
            if (loadedAssembly != null)
            {
                return loadedAssembly;
            }

            string strTempAssmbPath = "";

            strTempAssmbPath = args.Name.Substring(0, args.Name.IndexOf(","));

            string folderPath = Global.BuzzPath;
            string rawAssemblyFile = new AssemblyName(args.Name).Name;
            string rawAssemblyPath = Path.Combine(folderPath, rawAssemblyFile);

            string assemblyPath = rawAssemblyPath + ".dll";
            Assembly assembly = null;

            if (File.Exists(assemblyPath))
            {
                assembly = Assembly.LoadFile(assemblyPath);
            }

            return assembly;
        }

        public void ScanDlls()
        {
            MachineDLLsList = MachineDLLScanner.GetMachineDLLs(this);
        }

        internal void UpdateInstrumentList(MachineDatabase mdb)
        {
            InstrumentManager instrumentManager = new InstrumentManager();
            InstrumentList = instrumentManager.CreateInstrumentsList(this, mdb);
            PropertyChanged.Raise(this, "Instruments");
        }

        public void StartTimer()
        {
            dtVUMeter = new DispatcherTimer();
            dtVUMeter.Interval = TimeSpan.FromMilliseconds(1000 / 30);
            dtVUMeter.Tick += (sender, e) =>
            {
                if (maxSampleLeft >= 0)
                {
                    var db = Math.Min(Math.Max(Decibel.FromAmplitude(maxSampleLeft), -VUMeterRange), 0.0);
                    double left = (db + VUMeterRange) / VUMeterRange;
                    db = Math.Min(Math.Max(Decibel.FromAmplitude(maxSampleRight), -VUMeterRange), 0.0);
                    double right = (db + VUMeterRange) / VUMeterRange;

                    maxSampleLeft = -1;
                    maxSampleRight = -1;

                    if ((left >= 0) && (right >= 0) && (left != VUMeterLevel.Item1 || right != VUMeterLevel.Item2))
                    {
                        VUMeterLevel = new Tuple<double, double>(left, right);
                        PropertyChanged.Raise(this, "VUMeterLevel");
                    }
                }
            };
            dtVUMeter.Start();
        }

        public void ActivatePatternEditor()
        {
            NewSequenceEditorActivate = false;
            if (ActiveView != BuzzView.PatternView)
            {
                ActiveView = BuzzView.PatternView;
            }
            PatternEditorActivated?.Invoke();

            if (PatternEditorPattern != null)
            {
                var mc = PatternEditorPattern.Machine as MachineCore;
                var em = mc.EditorMachine;
                MachineManager.ActivateEditor(em);
            }
        }

        public void ActivateSequenceEditor()
        {
            NewSequenceEditorActivate = true;
            if (ActiveView != BuzzView.PatternView)
            {
                ActiveView = BuzzView.PatternView;
            }
            SequenceEditorActivated?.Invoke();
        }

        public void AddMachineDLL(string path, MachineType type)
        {
            try
            {
                string libName = Path.GetFileName(path);
                string libPath = Path.GetDirectoryName(path);
                var mDll = MachineDLLScanner.ValidateDll(this, libName, libPath);
                if (mDll != null)
                {
                    if (!machineDLLsList.ContainsKey(mDll.Name))
                    {
                        XMLMachineDLL[] mdxmlArray = [mDll];
                        MachineDLLScanner.AddMachineDllsToDictionary(mdxmlArray, machineDLLsList);
                        PropertyChanged.Raise(this, "MachineDLLs");
                    }
                }
            }
            catch { }

            MachineDB = new MachineDatabase(this);
            UpdateInstrumentList(MachineDB);
        }

        public bool CanExecuteCommand(BuzzCommand cmd)
        {
            if (cmd == BuzzCommand.Stop)
            {
                return true;
            }
            return true;
        }

        public void ConfigureAudioDriver()
        {
            AudioEngine.ShowControlPanel();
        }

        public void DCWriteLine(string s)
        {
            DebugStreamWrite.WriteLine(s);
        }

        public void ExecuteCommand(BuzzCommand cmd)
        {
            if (cmd == BuzzCommand.DebugConsole)
            {
                debugWindow.Show();
                debugWindow.BringToTop();
            }
            else if (cmd == BuzzCommand.About)
            {
                BuzzCommandRaised.Invoke(cmd);
            }
            else if (cmd == BuzzCommand.Exit)
            {
                if (Modified)
                {
                    var result = Utils.MessageBox("Save changes to " + (SongCore.SongName == null ? "Untitled" : SongCore.SongName), "ReBuzz", MessageBoxButton.YesNoCancel);
                    if (result == MessageBoxResult.Yes)
                    {
                        SaveSongFile(SongCore.SongName);
                    }
                    else if (result == MessageBoxResult.Cancel)
                    {
                        return;
                    }
                }
                playing = false;
                Release();
                Environment.Exit(0);
            }
            else if (cmd == BuzzCommand.Stop)
            {
                lock (AudioLock)
                {
                    Playing = Recording = false;
                    if (SoloPattern != null)
                    {
                        SoloPattern.IsPlayingSolo = false;
                    }
                }
            }
            else if (cmd == BuzzCommand.OpenFile)
            {
                OpenFileDialog openFileDialog = new OpenFileDialog();
                openFileDialog.Filter = "All songs (*.bmw, *.bmx, *bmxml)|*.bmw;*.bmx;*.bmxml|Songs with waves (*.bmx)|*.bmx|Songs without waves (*.bmw)|*.bmw|ReBuzz XML (*.bmxml)|*.bmxml";
                if (openFileDialog.ShowDialog() == true)
                {
                    OpenSongFile(openFileDialog.FileName);
                }
            }
            else if (cmd == BuzzCommand.SaveFile)
            {
                SaveSongFile(SongCore.SongName);
            }
            else if (cmd == BuzzCommand.SaveFileAs)
            {
                SaveSongFile(null);
            }
            else if (cmd == BuzzCommand.NewFile)
            {
                if (CheckSaveSong())
                {
                    NewSong();
                    BuzzCommandRaised?.Invoke(cmd);
                }
            }
            else if (cmd == BuzzCommand.Undo || cmd == BuzzCommand.Redo || cmd == BuzzCommand.Copy || cmd == BuzzCommand.Cut || cmd == BuzzCommand.Paste)
            {
                BuzzCommandRaised?.Invoke(cmd);
            }
            /*
            else if (cmd == BuzzCommand.Redo)
            {

            }
            else if (cmd == BuzzCommand.Copy)
            {

            }
            else if (cmd == BuzzCommand.Cut)
            {

            }
            else if (cmd == BuzzCommand.Paste)
            {
            }
            */
            else if (cmd == BuzzCommand.Preferences)
            {
                if (preferencesWindow == null)
                {
                    preferencesWindow = new PreferencesWindow(this);
                    var rd = Utils.GetUserControlXAML<ResourceDictionary>("MachineView\\MVResources.xaml");
                    preferencesWindow.Resources.MergedDictionaries.Add(rd);
                    if (preferencesWindow.ShowDialog() == true)
                    {
                        MidiInOutEngine.ReleaseAll();

                        List<int> midiIns = new List<int>();
                        foreach (var item in preferencesWindow.lbMidiInputs.Items)
                        {
                            var lbItem = item as PreferencesWindow.ControllerCheckboxVM;
                            if (lbItem.Checked)
                                midiIns.Add(lbItem.Id);
                        }
                        MidiEngine.SetMidiInputDevices(midiIns);
                        MidiInOutEngine.OpenMidiInDevices();


                        List<int> midiOuts = new List<int>();
                        foreach (var item in preferencesWindow.lbMidiOutputs.Items)
                        {
                            var lbItem = item as PreferencesWindow.ControllerCheckboxVM;
                            if (lbItem.Checked)
                                midiOuts.Add(lbItem.Id);
                        }
                        MidiEngine.SetMidiOutputDevices(midiOuts);
                        MidiInOutEngine.OpenMidiOutDevices();

                        MidiControllerAssignments.ClearAll();
                        foreach (PreferencesWindow.ControllerVM item in preferencesWindow.lvControllers.Items)
                        {
                            MidiControllerAssignments.Add(item.Name, item.Channel - 1, item.Controller, item.Value);
                        }

                        MIDIControllers = MidiControllerAssignments.GetMidiControllerNames().ToReadOnlyCollection();

                        AudioEngine.FinalStop();
                        AudioEngine.ReleaseAudioDriver();

                        long processorAffinity = preferencesWindow.GetProcessorAffinity();
                        RegistryEx.Write("ProcessorAffinity", processorAffinity, "Settings");

                        int threadType = preferencesWindow.cbAudioThreadType.SelectedIndex;
                        RegistryEx.Write("AudioThreadType", threadType, "Settings");

                        int threadCount = preferencesWindow.cbAudioThreads.SelectedIndex + 1;
                        RegistryEx.Write("AudioThreads", threadCount, "Settings");

                        int algorithm = preferencesWindow.cbAlgorithms.SelectedIndex;
                        RegistryEx.Write("WorkAlgorithm", algorithm, "Settings");

                        if (SelectedAudioDriver != null)
                        {
                            try
                            {
                                AudioEngine.CreateAudioOut(SelectedAudioDriver);
                                AudioEngine.Play();
                            }
                            catch { }
                        }

                        Utils.SetProcessorAffinityMask(true);
                    }
                    preferencesWindow = null;
                }
            }
        }

        internal void Release()
        {
            Global.EngineSettings.PropertyChanged -= EngineSettings_PropertyChanged;
            Global.GeneralSettings.PropertyChanged -= GeneralSettings_PropertyChanged;

            MidiControllerAssignments.Song = null;

            timerAutomaticBackups.Stop();
            MidiInOutEngine.ReleaseAll();
            AudioEngine.FinalStop();
            DeleteBackup();
        }

        private bool CheckSaveSong()
        {
            if (Modified)
            {
                var result = Utils.MessageBox("Save changes to " + (SongCore.SongName == null ? "Untitled" : SongCore.SongName), "ReBuzz", MessageBoxButton.YesNoCancel);
                if (result == MessageBoxResult.Yes)
                {
                    SaveSongFile(SongCore.SongName);
                    return true;
                }
                else if (result == MessageBoxResult.Cancel)
                {
                    return false;
                }
            }

            return true;
        }

        private void UpdateRecentFilesList(string fileName)
        {
            var files = RegistryEx.ReadNumberedList<string>("File", "Recent File List").ToList();
            foreach (var file in files.ToArray())
            {
                if (file == fileName)
                {
                    files.Remove(file);
                }
            }

            // Move to top
            files.Insert(0, fileName);

            try
            {
                Registry.CurrentUser.DeleteSubKeyTree(Global.RegistryRoot + "\\" + "Recent File List");
            }
            catch { }

            RegistryKey regkey = Registry.CurrentUser.CreateSubKey(Global.RegistryRoot + "\\" + "Recent File List");
            int maxFiles = Math.Min(files.Count, 10);
            for (int i = 0; i < maxFiles; i++)
            {
                regkey.SetValue("File" + (i + 1).ToString(), files[i]);
            }
        }

        public IMachine GetIMachineFromCMachinePtr(IntPtr pmac)
        {
            return Song.Machines.FirstOrDefault(m => m.CMachinePtr == pmac);
        }

        internal bool NewSequenceEditorActivate { get; set; }
        public void NewSequenceEditorActivated()
        {
            NewSequenceEditorActivate = true;
        }

        internal event Action<FileEventType, string> FileEvent;
        internal event Action<string> OpenFile;

        public void OpenSongFile(string filename)
        {
            if (CheckSaveSong())
            {
                masterLoading = true;
                AudioEngine.Stop();

                DeleteBackup();
                NewSong();

                lock (AudioLock)
                {
                    SkipAudio = true;
                    bool playing = Playing;
                    Playing = false;
                }

                IReBuzzFile bmxFile = GetReBuzzFile(filename);

                bmxFile.FileEvent += (type, eventText) =>
                {
                    FileEvent?.Invoke(type, eventText);
                };

                if (false)
                {
                    // Test
                    OpenFile.Invoke(filename);
                    bmxFile.Load(filename);
                }
                else
                {
                    try
                    {
                        bmxFile.FileEvent += (type, eventText) =>
                        {
                            FileEvent?.Invoke(type, eventText);
                        };

                        OpenFile.Invoke(filename);
                        bmxFile.Load(filename);
                    }
                    catch (Exception e)
                    {

                        MessageBox.Show(e.InnerException.Message, "Error loading " + filename);
                        bmxFile.EndFileOperation();
                        NewSong();
                        SkipAudio = false;
                        return;
                    }
                }

                SongCore.SongName = filename;
                UpdateRecentFilesList(filename);
                if (OpenSong != null)
                {
                    OpenSongCore os = new OpenSongCore();
                    os.Song = songCore;
                    var subSelections = bmxFile.GetSubSections();
                    foreach (var sub in subSelections)
                    {
                        os.AddStream(sub.Key, sub.Value);
                    }

                    OpenSong.Invoke(os);
                }
                SongCore.PlayPosition = 0;

                SkipAudio = false;
                Modified = false;
                AudioEngine.Play();
                //Playing = playing;
            }
        }

        internal void ImportSong(float x, float y)
        {
            OpenFileDialog openFileDialog = new OpenFileDialog();
            openFileDialog.Filter = "All songs (*.bmw, *.bmx, *bmxml)|*.bmw;*.bmx;*.bmxml|Songs with waves (*.bmx)|*.bmx|Songs without waves (*.bmw)|*.bmw|ReBuzz XML (*.bmxml)|*.bmxml";
            if (openFileDialog.ShowDialog() == true)
            {
                SkipAudio = true;
                bool playing = Playing;
                Playing = false;

                // Let other threads react to changes
                Thread.Sleep(10);

                IReBuzzFile bmxFile = GetReBuzzFile(openFileDialog.FilterIndex);
                var filename = openFileDialog.FileName;

                try
                {
                    bmxFile.Load(filename, x, y, true);
                }
                catch (Exception ex)
                {
                    Utils.MessageBox("Error importing file " + filename + "\n\n" + ex.ToString(), "Error importing file.");
                }

                SkipAudio = false;
                Playing = playing;
            }
        }

        IReBuzzFile GetReBuzzFile(int filterIndex)
        {
            IReBuzzFile file;
            if (filterIndex == 1 || filterIndex == 2)
            {
                file = new BMXFile(this);
            }
            else
            {
                file = new BMXMLFile(this);
            }
            return file;
        }

        IReBuzzFile GetReBuzzFile(string path)
        {
            IReBuzzFile file;
            string extension = Path.GetExtension(path);
            if (extension == ".bmx" || extension == ".bmw")
            {
                file = new BMXFile(this);
            }
            else
            {
                file = new BMXMLFile(this);
            }
            return file;
        }

        public void SaveSongFile(string filename)
        {
            DeleteBackup();

            // Check filename
            if (filename == null)
            {
                SaveFileDialog saveFileDialog = new SaveFileDialog();
                saveFileDialog.Filter = "Songs with waves (*.bmx)|*.bmx|Songs without waves (*.bmw)|*.bmw|ReBuzz XML (*.bmxml)|*.bmxml";
                if (saveFileDialog.ShowDialog() == true)
                {
                    filename = saveFileDialog.FileName;
                    songCore.SongName = filename;
                    UpdateRecentFilesList(filename);
                }
                else
                {
                    return;
                }
            }

            // Do save
            IReBuzzFile file = GetReBuzzFile(filename);

            SaveSongCore ss = new SaveSongCore();
            ss.Song = songCore;
            SaveSong.Invoke(ss);

            file.SetSubSections(ss);

            lock (AudioLock)
            {
                try
                {
                    file.Save(filename);
                }
                catch (Exception ex)
                {
                    Utils.MessageBox("Error saving file " + filename + "\n\n" + ex.ToString(), "Error saving file.");
                }
            }

            Modified = false;
        }

        public int RenderAudio(float[] buffer, int nsamples, int samplerate)
        {
            var ap = AudioEngine.AudioProvider;
            ap.ReadOverride(buffer, 0, nsamples);
            return nsamples;
        }

        public void SendMIDIInput(int data)
        {
            if (midiFocusMachine != null && !midiFocusMachine.DLL.IsMissing)
            {
                var editor = (midiFocusMachine as MachineCore).EditorMachine;
                bool polacConversion = (midiFocusMachine.DLL.Name == "Polac VSTi 1.1") ||
                    (midiFocusMachine.DLL.Name == "Polac VST 1.1");

                if (editor == null)
                {
                    // Send to machine
                    MachineManager.SendMidiInput(midiFocusMachine, data, polacConversion);
                }
                else
                {
                    // Send to editor. Editor will send midi message to machine.
                    MachineManager.SendMidiInput(editor, data, polacConversion);
                }
            }
            MIDIActivity = true;

            //MIDIInput?.Invoke(data);
            Application.Current.Dispatcher.BeginInvoke(new Action(() =>
                MIDIInput?.Invoke(data)
            ));
        }

        public IEnumerable<Tuple<int, string>> GetMidiOuts()
        {
            var mos = MidiInOutEngine.GetMidiOutputDevices();
            List<Tuple<int, string>> moList = new List<Tuple<int, string>>();
            foreach (var mo in mos)
            {
                var di = MidiOut.DeviceInfo(mo);
                moList.Add(new Tuple<int, string>(mo, di.ProductName));
            }

            return moList;
        }

        public void SendMIDIOutput(int device, int data)
        {
            MidiInOutEngine.SendMidiOut(device, data);
        }

        public event Action<UserControl> SetPatternEditorControl;

        public void SetPatternEditorMachine(IMachine m)
        {
            PatternEditorMachine = m;
            UserControl patternEditorControl;
            MachineCore mc;
            if (m != null)
            {
                mc = m as MachineCore;
            }
            else
            {
                mc = Song.Machines.Last() as MachineCore;
            }

            if (mc.DLL.IsMissing)
                return;

            var em = mc.EditorMachine;
            patternEditorControl = MachineManager.GetPatternEditorControl(em);

            if (SetPatternEditorControl != null)
            {
                SetPatternEditorControl.Invoke(patternEditorControl);
            }
        }

        public void SetPatternEditorPattern(IPattern p)
        {
            PatternEditorPattern = p;
            var mc = p.Machine as MachineCore;

            if (mc.DLL.IsMissing)
                return;

            var em = mc.EditorMachine;
            if (em == null)
            {
                // Create editor
                CreateEditor(mc, null, null);
                em = mc.EditorMachine;
            }
            UserControl patternEditorControl = MachineManager.GetPatternEditorControl(em);
            // Calling this first time will link editor machine to machine owning pattern p.
            // Make "AssingEditorToMachineMethod"?
            MachineManager.SetPatternEditorPattern(em, p);
            if (SetPatternEditorControl != null && patternEditorControl != null)
            {
                SetPatternEditorControl.Invoke(patternEditorControl);
            }
        }


        public void TakePerformanceDataSnapshot()
        {
            lock (AudioLock)
            {
                PerformanceData = new BuzzPerformanceData();
                foreach (var m in Song.Machines)
                {
                    var machine = m as MachineCore;
                    var pd = new MachinePerformanceData();
                    var pdc = machine.PerformanceDataCurrent;
                    pd.MaxEngineLockTime = pdc.MaxEngineLockTime;
                    pd.PerformanceCount = pdc.PerformanceCount;
                    pd.CycleCount = pdc.CycleCount * 1000;
                    pd.SampleCount = pdc.SampleCount;

                    machine.PerformanceData = pd;
                    pdc.MaxEngineLockTime = 0;
                }

                long now = DateTime.Now.Ticks;
                PerformanceCurrent.PerformanceCount += now - performanceCountTime;
                performanceCountTime = now;

                PerformanceData.EnginePerformanceCount = PerformanceCurrent.EnginePerformanceCount;
                PerformanceData.PerformanceCount = PerformanceCurrent.PerformanceCount;
            }
        }

        const double VUMeterRange = 80.0;
        float maxSampleLeft = -1;
        float maxSampleRight = -1;

        internal Gear Gear { get; }

        internal void MasterTapSamples(float[] resSamples, int offset, int count)
        {
            var s = GetSongTime();
            float scale = (1.0f / 32768.0f);
            for (int i = 0; i < count; i += 2)
            {
                maxSampleLeft = Math.Max(maxSampleLeft, resSamples[offset + i] * scale);
                maxSampleRight = Math.Max(maxSampleRight, resSamples[offset + i + 1] * scale);
            }

            float[] samples = new float[count];
            for (int i = 0; i < count; i++)
            {
                samples[i] = resSamples[offset + i];
            }

            Application.Current.Dispatcher.BeginInvoke(new Action(() =>
            {
                if (MasterTap != null)
                {
                    MasterTap.Invoke(samples, true, s);
                }
            }
            ));
        }

        internal MachineCore CloneMachine(MachineCore machineToClone, float x, float y)
        {
            MachineCore machine = null;
            var instInfo = MachineDB.DictLibRef.Values.FirstOrDefault(i => i.InstrumentFullName == machineToClone.InstrumentName);

            var MachineDll = MachineDLLs[instInfo.libName];

            Modified = true;
            return machine;
        }

        internal MachineCore CreateMachine(int id, float x, float y)
        {
            MachineCore machine = null;
            if (MachineDB.DictLibRef.ContainsKey(id))
            {
                var instInfo = MachineDB.DictLibRef[id];
                var MachineDll = MachineDLLs[instInfo.libName];
                machine = MachineManager.CreateMachine(MachineDll.Name, MachineDll.Path, instInfo.InstrumentFullName,
                    null, MachineDll.Info.MinTracks, x, y, false);

                // CreateEditor(machine, null, null);

                MIDIFocusMachine = machine;
                Modified = true;
            }

            return machine;
        }

        internal MachineCore CreateMachine(string machine, string instrument, string name, byte[] data,
            string patternEditor, byte[] patterneditordata, int trackcount, float x, float y, string editorName = null)
        {
            if (machine == "Master")
                return null;

            string path = null;
            if (MachineDLLs.ContainsKey(machine))
            {
                var MachineDll = MachineDLLs[machine];
                path = MachineDll.Path;

                if (trackcount < 0)
                    trackcount = MachineDll.Info.MinTracks;
            }

            if (trackcount <= 0)
                trackcount = 1;

            MachineCore machineCore = MachineManager.CreateMachine(machine, path, instrument, data, trackcount, x, y, false, name, !songCore.Importing);

            if (songCore.Importing)
            {
                songCore.DictInitData[machineCore] = new MachineInitData() { data = data, tracks = trackcount };
            }

            if (machineCore != null && patternEditor != null)
            {
                CreateEditor(machineCore, patternEditor, patterneditordata, editorName);
            }

            MIDIFocusMachine = machineCore;

            Modified = true;
            return machineCore;
        }

        internal void CreateEditor(MachineCore machineToUseEditor, string patternEditor, byte[] patterneditordata, string editorName = null)
        {
            // Create pattern editor but keep it hidden
            MachineCore peMachine = null;
            IMachineDLL editorMachineDll = null;

            if (patternEditor != null)
            {
                if (MachineDLLs.ContainsKey(patternEditor))
                {
                    editorMachineDll = MachineDLLs[patternEditor];
                }
                else
                {
                    if (!Gear.HasSameDataFormat(patternEditor, DefaultPatternEditor))
                    {
                        // Clear pattern editor data if not compatible with MPE
                        patterneditordata = null;
                    }
                    editorMachineDll = MachineDLLs[DefaultPatternEditor];
                }
            }
            else
            {
                editorMachineDll = MachineDLLs[DefaultPatternEditor];
            }
            string name = editorName == null ? GetNewEditorName() : editorName;
            peMachine = MachineManager.CreateMachine(editorMachineDll.Name, editorMachineDll.Path, null, patterneditordata, editorMachineDll.Info.MinTracks, 0, 0, true, name);

            // Connect editor to master
            var master = SongCore.Machines.FirstOrDefault(m => m.DLL.Info.Type == MachineType.Master);
            new ConnectMachinesAction(this, peMachine, master, 0, 0, 0x4000, 0x4000).Do();

            // Link machine to editor. Maybe specific call?
            MachineManager.SetPatternEditorPattern(machineToUseEditor, machineToUseEditor.Patterns.FirstOrDefault());
            machineToUseEditor.EditorMachine = peMachine;
        }

        public string GetNewEditorName()
        {
            string name = ((char)1) + "pe";
            int num = 1;
            while (songCore.MachinesList.FirstOrDefault(m => m.Name == (name + num)) != null)
            {
                num++;
            }

            return name + num;
        }

        public static object AudioLock = new object();
        public MachineCore CreateMaster()
        {
            var machine = MachineManager.GetMaster(this);
            MIDIFocusMachine = machine;
            AddMachine(machine);

            // Create editor for Master
            CreateEditor(machine, DefaultPatternEditor, null);
            machine.Ready = true;

            var masterGlobalParameters = machine.ParameterGroups[1].Parameters;
            masterGlobalParameters[0].SubscribeEvents(0, MasterVolumeChanged, null);
            masterGlobalParameters[1].SubscribeEvents(0, BPMChanged, null);
            masterGlobalParameters[2].SubscribeEvents(0, TPBChanged, null);

            Modified = false;
            return machine;
        }

        private void TPBChanged(IParameter parameter, int track)
        {
            TPB = parameter.GetValue(track);
        }

        private void BPMChanged(IParameter parameter, int track)
        {
            BPM = parameter.GetValue(track);
        }

        private void MasterVolumeChanged(IParameter parameter, int track)
        {
            MasterVolume = (parameter.MaxValue - parameter.GetValue(track)) / (double)parameter.MaxValue;

            // Modified flag active.
            masterLoading = false;
        }

        public MachineCore GetMachineFromHostID(long id)
        {
            return SongCore.MachinesList.FirstOrDefault(m => m.CMachineHost == id);
        }

        internal bool RenameMachine(MachineCore machine, string name)
        {
            lock (AudioLock)
            {
                if (machine.Name != name)
                {
                    string oldName = machine.Name;
                    machine.Name = MachineManager.GetNewMachineName(name);
                    Modified = true;

                    return true;
                }
                return false;
            }
        }

        internal void SetModifyFlag(MachineCore machine)
        {
            Modified = true;
        }

        internal void AddMachine(MachineCore machine)
        {
            lock (AudioLock)
            {
                SongCore.MachinesList.Add(machine);
                if (!machine.Hidden)
                {
                    // Make visible in UI
                    SongCore.InvokeMachineAdded(machine);
                }
            }
        }

        internal void RemoveMachine(MachineCore machine)
        {
            if (machine.DLL.Info.Type == MachineType.Master)
                return;

            lock (AudioLock)
            {
                foreach (var seq in SongCore.Sequences.Where(s => s.Machine == machine).ToArray())
                {
                    SongCore.RemoveSequence(seq);
                }
                machine.CloseWindows();

                // Disconnect editor machine
                if (machine.EditorMachine != null)
                {
                    foreach (var mc in machine.EditorMachine.AllOutputs.ToArray())
                    {
                        (mc.Destination as MachineCore).AllInputs.Remove(mc);
                        machine.EditorMachine.AllOutputs.Clear();
                    }
                }

                foreach (var pattern in machine.PatternsList.ToArray())
                {
                    machine.DeletePattern(pattern);
                    pattern.ClearEvents();
                }
            }

            SongCore.RemoveMachine(machine.EditorMachine);
            SongCore.RemoveMachine(machine);

            MachineManager.DeleteMachine(machine.EditorMachine);
            MachineManager.DeleteMachine(machine);
            machine.EditorMachine = null;
            SetPatternEditorMachine(SongCore.Machines.Last());

        }

        public void SetModifiedFlag()
        {
            Modified = true;
        }

        internal void RecordControlChange(ParameterCore parameter, int track, int value)
        {
            var machine = parameter.Group.Machine as MachineCore;
            var peMachine = machine.EditorMachine;
            if (Recording && peMachine != null)
            {
                // No need to do this?
                //parameter.SetValue(track | 1 << 16, value);
                MachineManager.RecordContolChange(peMachine, parameter, track, value);
            }
        }

        internal void PlayCurrentEditorPattern()
        {
            PatternEditorPattern.IsPlayingSolo = true;
            Playing = true;
        }

        // Clean up song 
        internal void NewSong()
        {
            SkipAudio = true;
            Playing = false;

            // Create status window
            OpenFile.Invoke("Closing song...");

            DeleteBackup();
            lock (AudioLock)
            {
                var master = SongCore.MachinesList.First();

                FileEvent?.Invoke(FileEventType.StatusUpdate, "Remove Connections...");
                // Remove connections
                foreach (var machine in SongCore.MachinesList)
                {
                    // Remove connections
                    foreach (var input in machine.AllInputs.ToArray())
                    {
                        new DisconnectMachinesAction(this, input).Do();
                    }
                }

                // Remove machines
                foreach (var machine in SongCore.MachinesList.ToArray())
                {
                    machine.CloseWindows();
                    if (machine != master)
                    {
                        if (!machine.Hidden)
                            FileEvent?.Invoke(FileEventType.StatusUpdate, "Remove Machine " + machine.Name);

                        // Remove machine
                        RemoveMachine(machine);

                        if (machine == master.EditorMachine)
                        {
                            master.EditorMachine = null;
                        }
                    }
                    else
                    {
                        // Master
                        foreach (var pattern in machine.Patterns.ToArray())
                            machine.DeletePattern(pattern);

                        foreach (var seq in SongCore.Sequences.Where(s => s.Machine == machine).ToArray())
                        {
                            SongCore.RemoveSequence(seq);
                        }
                    }
                }


                FileEvent?.Invoke(FileEventType.StatusUpdate, "Clear Wavetable...");
                // Clear Wavetable
                for (int i = 0; i < songCore.WavetableCore.WavesList.Count; i++)
                {
                    var wave = songCore.WavetableCore.WavesList[i];
                    if (wave != null)
                    {
                        songCore.WavetableCore.LoadWave(i, null, null, false);
                    }
                }

                // Clear pattern and sequece
                master.PatternsList.Clear();
                FileEvent?.Invoke(FileEventType.Close, "Done.");
            }

            // Center Master position
            List<Tuple<IMachine, Tuple<float, float>>> moveList = new List<Tuple<IMachine, Tuple<float, float>>>
            {
                new Tuple<IMachine, Tuple<float, float>>(Song.Machines.First(), new Tuple<float, float>(0, 0))
            };
            SongCore.MoveMachines(moveList);
            songCore.SongName = null;

            SongCore.ActionStack = new ManagedActionStack();
            SongCore.PlayPosition = 0;
            SongCore.LoopStart = 0;
            SongCore.LoopEnd = 16;
            SongCore.SongEnd = 16;
            TPB = 4;
            BPM = 126;
            Modified = false;

            GCSettings.LargeObjectHeapCompactionMode = GCLargeObjectHeapCompactionMode.CompactOnce;
            GC.Collect();
            GC.WaitForFullGCComplete();
            SkipAudio = false;
        }

        PlayWaveData PlayWaveData { get; set; }
        string infoText;
        private bool masterLoading;

        public string InfoText { get => infoText; internal set { infoText = value; PropertyChanged.Raise(this, "InfoText"); } }

        public AudioEngine AudioEngine { get; internal set; }
        public string DefaultPatternEditor { get; internal set; }
        public static bool SkipAudio;
        internal IMachine PatternEditorMachine { get; set; }

        internal bool IsPlayingWave()
        {
            return PlayWaveData != null;
        }

        internal void PlayWave(WaveCore wave, int start, int end, LoopType looptype)
        {
            lock (AudioLock)
            {
                StopPlayingWave();
                PlayWaveData = new PlayWaveData()
                {
                    wave = wave,
                    postion = 0,
                    start = start,
                    end = end,
                    looptype = looptype
                };
            }
        }

        internal void PlayWave(string file)
        {
            lock (AudioLock)
            {
                StopPlayingWave();
                var sf = SoundFile.OpenRead(file);
                PlayWaveData = new PlayWaveData()
                {
                    wave = null,
                    postion = 0,
                    start = 0,
                    end = 0,
                    looptype = LoopType.None,
                    sf = sf
                };
            }
        }

        internal void StopPlayingWave()
        {
            lock (AudioLock)
            {
                if (PlayWaveData != null && PlayWaveData.sf != null)
                {
                    PlayWaveData.sf.Dispose();
                }
                PlayWaveData = null;
            }
        }

        static int ReadBufferSize = 256;
        float[] waveFilebuffer = new float[ReadBufferSize];

        // Read sampleCount stereo samples to buffer
        internal bool GetPlayWaveSamples(float[] buffer, int offset, int sampleCount)
        {
            if (PlayWaveData != null)
            {
                if (PlayWaveData.sf != null)
                {
                    var sf = PlayWaveData.sf;
                    int samplesRead = 0;
                    int outOffset = 0;
                    
                    while (sampleCount > 0)
                    {
                        int maxReadFrameCount = ReadBufferSize / sf.ChannelCount;
                        int readFrameCount = Math.Min(maxReadFrameCount, sampleCount);
                        var n = sf.ReadFloat(waveFilebuffer, readFrameCount);
                        if (n <= 0) return false;

                        int inOffset = 0;
                        for (int j = 0; j < readFrameCount; j++)
                        {
                            for (int ch = 0; ch < sf.ChannelCount; ch++)
                            {
                                if (ch == 0)
                                {
                                    buffer[outOffset] = waveFilebuffer[inOffset];

                                    // Conver mono to stereo
                                    if (sf.ChannelCount == 0)
                                    {
                                        buffer[outOffset + 1] = waveFilebuffer[inOffset];
                                    }
                                }
                                else if (ch == 1)
                                {
                                    buffer[outOffset + 1] = waveFilebuffer[inOffset + 1];
                                }
                                else
                                {
                                    break;
                                }
                            }
                            outOffset += 2;
                            inOffset += sf.ChannelCount;
                        }
                        sampleCount -= readFrameCount;
                    }
                }
                else if (PlayWaveData.wave.Layers.Count > 0)
                {
                    var layer = PlayWaveData.wave.Layers.First();
                    layer.GetDataAsFloat(buffer, offset, 2, 0, PlayWaveData.postion, sampleCount);
                    layer.GetDataAsFloat(buffer, offset + 1, 2, 1, PlayWaveData.postion, sampleCount);
                    PlayWaveData.postion += sampleCount;
                    return true;
                }
            }
            return false;
        }

        internal SongTime GetSongTime()
        {
            SongTime songTime = new SongTime();
            songTime.CurrentTick = Song.PlayPosition;
            songTime.CurrentSubTick = subTickInfo.CurrentSubTick;
            songTime.PosInSubTick = subTickInfo.PosInSubTick;
            songTime.SamplesPerSec = SelectedAudioDriverSampleRate;
            songTime.BeatsPerMin = BPM;
            songTime.PosInTick = masterInfo.PosInTick;
            songTime.SamplesPerSubTick = subTickInfo.SamplesPerSubTick;
            songTime.TicksPerBeat = TPB;
            songTime.SubTicksPerTick = subTickInfo.SubTicksPerTick;
            return songTime;
        }

        internal void PatternEditorCommand(BuzzCommand cmd)
        {
            if (PatternEditorPattern != null)
            {
                var machine = PatternEditorPattern.Machine as MachineCore;

                MachineManager.Command(machine.EditorMachine, (int)cmd);
            }
        }

        public event Action<float[], int> AudioReceived;
        internal void AudioInputAvalable(float[] samples, int n)
        {
            AudioReceived?.Invoke(samples, n);
        }

        public void AudioOut(int channel, Sample[] samples, int n)
        {
            var AudioProvider = AudioEngine.GetAudioProvider();
            AudioProvider?.AudioSampleProvider.FillChannel(channel, samples, n);
        }

        internal bool SetEditorMachineForCurrent(IMachineDLL editorMachine)
        {
            bool ret = false;
            if (PatternEditorPattern != null && PatternEditorPattern.Machine.PatternEditorDLL != editorMachine)
            {
                ret = false;
                var machine = PatternEditorPattern.Machine as MachineCore;
                byte[] data = null;
                var currentEditorMachine = machine.EditorMachine;
                // Change it
                if (Gear.HasSameDataFormat(machine.EditorMachine.DLL.Name, editorMachine.Name))
                    data = machine.EditorMachine.Data;

                lock (ReBuzzCore.AudioLock)
                {
                    try
                    {
                        CreateEditor(machine, editorMachine.Name, data);
                        RemoveMachine(currentEditorMachine);
                    }
                    catch (Exception)
                    {
                        machine.EditorMachine = currentEditorMachine;
                        ret = true;
                    }
                }

                if (PatternEditorPattern != null)
                    SetPatternEditorPattern(PatternEditorPattern);
            }
            return ret;
        }
    }

    public class MasterInfoExtended : MasterInfo
    {
        public double AverageSamplesPerTick { get; internal set; }
    }

    public class SubTickInfoExtended : SubTickInfo
    {
        public double AverageSamplesPerSubTick { get; internal set; }
        public int SubTickReminderCounter { get; internal set; }
    }

    internal class PlayWaveData
    {
        public WaveCore wave;
        public int postion;
        public int start;
        public int end;
        public LoopType looptype;
        internal SoundFile sf;
    }
}
