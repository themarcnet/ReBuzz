#pragma once

using namespace  System::Runtime::InteropServices;

namespace ReBuzz
{
    namespace NativeMachineFramework
    {

        template<class T>
        class RefClassWrapper
        {
        public:
            inline RefClassWrapper() : m_classrefPtr(0)
            {}

            inline RefClassWrapper(T^ ref) : m_classrefPtr(0)
            {
                assign(ref);
            }

            virtual ~RefClassWrapper()
            {
                Free();
            }

            inline RefClassWrapper& operator = (T^ x)
            {
                Assign(x);
                return *this;
            }

            inline bool isNull() const
            {
                return m_classrefPtr == 0;
            }

            inline void Assign(T^ ref)
            {
                Free();
                assign(ref);
            }


            inline void Free()
            {
                if (m_classrefPtr != 0)
                {
                    GCHandle handle = GCHandle::FromIntPtr(IntPtr(m_classrefPtr));
                    handle.Free();
                    m_classrefPtr = 0;
                }
            }


            inline T^ GetRef()
            {
                if (m_classrefPtr == 0)
                    return nullptr;

                GCHandle handle = GCHandle::FromIntPtr(IntPtr(m_classrefPtr));
                return (T^)handle.Target;
            }

        private:

            inline void assign(T^ ref)
            {
                if (ref != nullptr)
                {
                    GCHandle handle = GCHandle::Alloc(ref);
                    m_classrefPtr = GCHandle::ToIntPtr(handle).ToPointer();
                }
            }

            void* m_classrefPtr;
        };


        //========================================================================

        template<class T>
        class AllocRefClassWrapper : RefClassWrapper<T>
        {
        public:
            inline AllocRefClassWrapper(T^ ref) : RefClassWrapper(ref)
            {}

            virtual ~AllocRefClassWrapper()
            {
                T^ ref = GetRef();
                if (ref != nullptr)
                    delete ref;

                Free();
            }
        };
    }
}