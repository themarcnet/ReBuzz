using System.Windows;


namespace NativeMachineFrameworkUI
{
    /// <summary>
    /// Interaction logic for UserControl1.xaml
    /// </summary>
    public partial class WinFormsControl : System.Windows.Controls.UserControl
    {
        public WinFormsControl(System.Windows.Forms.UserControl control)
        {
            _control = control;
            InitializeComponent();
        }

        

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            // Create the interop host control.
            System.Windows.Forms.Integration.WindowsFormsHost host =
                new System.Windows.Forms.Integration.WindowsFormsHost();

            // Assign the MaskedTextBox control as the host control's child.
            host.Child = _control;

            // Add the interop host control to the Grid
            // control's collection of child controls.
            this.NativeMachineGrid.Children.Add(host);

            //Resize
            resize();
        }

        private void resize()
        {
            if (this.NativeMachineGrid.Children.Count != 0)
            {
                this.NativeMachineGrid.Children[0].SetValue(Window.WidthProperty, this.ActualWidth);
                this.NativeMachineGrid.Children[0].SetValue(Window.HeightProperty, this.ActualHeight);
            }
        }

        private void UserControl_SizeChanged(object sender, RoutedEventArgs e)
        {
            resize();
        }

        private System.Windows.Forms.UserControl _control;
    }

}
