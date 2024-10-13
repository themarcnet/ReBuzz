#pragma once

#include <map>
#include <memory>
#include <mutex>
#include "RefClassWrapper.h"

#include <mutex>

namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        typedef void (*OnNewBuzzLookupItemCallback)(void* item, void* param);

        template<class RebuzzType, class BuzzTypeEmulation,  typename BuzzType>
        class RebuzzBuzzLookup
        {
        public:
            RebuzzBuzzLookup(OnNewBuzzLookupItemCallback onNewItemCallback,
                            void * callbackparam)  : m_onNewItemCallback(onNewItemCallback),
                                                     m_callbackParam(callbackparam)
            {}

            ~RebuzzBuzzLookup()
            {}

            BuzzType* GetBuzzTypeById(uint64_t id) const
            {
                BuzzType* ret = NULL;

                std::lock_guard<std::mutex> lg(m_lock);
                
                ret = GetBuzzTypeById_Internal(id);
                
                return ret;
            }

            RebuzzType^ GetReBuzzTypeByBuzzType(BuzzType* bt)
            {
                std::lock_guard<std::mutex> lg(m_lock);

                //Get the id
                const auto& found = m_buzzToIdMap.find(bt);
                if (found == m_buzzToIdMap.end())
                    return nullptr;

                //Get the ref
                const auto& foundref = m_idToRefMap.find((*found).second);
                if (foundref == m_idToRefMap.end())
                    return nullptr;

                return (*foundref).second->GetRef();
            }

            BuzzTypeEmulation* GetBuzzEmulationType(BuzzType* bt)
            {
                std::lock_guard<std::mutex> lg(m_lock);

                //Get the id
                const auto& found = m_buzzToIdMap.find(bt);
                if (found == m_buzzToIdMap.end())
                    return nullptr;

                //Get the take type
                const auto& founddata = m_idToDataMap.find((*found).second);
                if (founddata == m_idToDataMap.end())
                    return nullptr;

                return (*founddata).second.get();
            }

            BuzzType* GetOrStoreReBuzzTypeById(uint64_t id, RebuzzType^ buzztype)
            {
                BuzzType* ret = NULL;
                bool callCallback = false;
                {
                    std::lock_guard<std::mutex> lg(m_lock);

                    ret = GetBuzzTypeById_Internal(id);
                    if (ret == NULL)
                    {
                        ret = StoreReBuzzTypeById_Internal(id, buzztype);
                        callCallback = true; //New item saved, call the callback after releasing the mutex
                    }
                }

                if (callCallback && (m_onNewItemCallback != NULL))
                {
                    m_onNewItemCallback(ret, m_callbackParam);
                }

                return ret;
            }

        private:
            BuzzType* GetBuzzTypeById_Internal(uint64_t id) const
            {
                BuzzType* ret = NULL;

                const auto& found = m_idToDataMap.find(id);
                if (found == m_idToDataMap.end())
                {
                    return NULL;
                }

                return reinterpret_cast<BuzzType*>((*found).second.get());
            }

            BuzzType* StoreReBuzzTypeById_Internal (uint64_t id, RebuzzType^ ref)
            {
                BuzzType* ret = NULL;

                m_idToRefMap[id].reset(new RefClassWrapper< RebuzzType>(ref));
                m_idToDataMap[id].reset(new BuzzTypeEmulation());
                    
                ret = (BuzzType*)reinterpret_cast<BuzzType*>(m_idToDataMap[id].get());
                m_buzzToIdMap[ret] = id;
                
                return ret;
            }


            mutable std::mutex m_lock;
            std::map<uint64_t, std::shared_ptr<RefClassWrapper<RebuzzType>>> m_idToRefMap;
            std::map<uint64_t, std::shared_ptr<BuzzTypeEmulation>> m_idToDataMap;
            std::map<BuzzType*, uint64_t> m_buzzToIdMap;
            OnNewBuzzLookupItemCallback m_onNewItemCallback;
            void* m_callbackParam;
        };
    }
}
