SRCS  = CGUIWindowMN.cpp
SRCS += CGUIWindowMNDemand.cpp
SRCS += MNJSONOperations.cpp
SRCS += NWClient.cpp
SRCS += NWClientUtilities.cpp
SRCS += NWMediaManager.cpp
SRCS += NWPurgeManager.cpp
SRCS += NWPlayer.cpp
SRCS += NWTVAPI.cpp

LIB   = nwmn.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
