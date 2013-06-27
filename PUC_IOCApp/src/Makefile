TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = PUC_IOC
# PUC_IOC.dbd will be created and installed
DBD += PUC_IOC.dbd

# PUC_IOC.dbd will be made up from these files:
PUC_IOC_DBD += base.dbd
PUC_IOC_DBD += puc.dbd

# Include dbd files from all support applications:
#PUC_IOC_DBD += xxx.dbd

# Add all the support libraries needed by this IOC
#PUC_IOC_LIBS += xxx

# PUC_IOC_registerRecordDeviceDriver.cpp derives from PUC_IOC.dbd
PUC_IOC_SRCS += PUC_IOC_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
PUC_IOC_SRCS_DEFAULT += PUC_IOCMain.cpp
PUC_IOC_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#PUC_IOC_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
PUC_IOC_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_LIBS += stream
PROD_LIBS += asyn
#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

