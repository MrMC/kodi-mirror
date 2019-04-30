set(SOURCES CGUIWindowMN.cpp
            CGUIWindowMNDemand.cpp
            MNJSONOperations.cpp
            NWClient.cpp
            NWClientUtilities.cpp
            NWMediaManager.cpp
            NWPlayer.cpp
            NWPurgeManager.cpp
            NWTVAPI.cpp)

set(HEADERS CGUIWindowMN.h
            CGUIWindowMNDemand.h
            MNJSONOperations.h
            NWClient.h
            NWClientUtilities.h
            NWMediaManager.h
            NWPlayer.h
            NWPurgeManager.h
            NWTVAPI.h)

core_add_library(nwmn)
