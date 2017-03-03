# ****************************************************************************
# *
# *  Makefile Builds Comms Handler Process / Executable
# *
# *****************************************************************************



# *****************************************************************************
# Add the target name of the output to be produced
# *****************************************************************************

APP_NAME := pi-netTest

TOPDIR=$(shell pwd)

PROJDIR=$(TOPDIR)

SRCDIR=$(TOPDIR)/src

INCDIR=$(TOPDIR)/include

IPERFDIR = /home/pi/Downloads/iperf/src

INCDIRS := -I$(SRCDIR) -I$(INCDIR) -I$(OPTDIR)/include -I$(IPERFDIR)

ARMDIR = /usr/lib/arm-linux-gnueabihf

LOCALDIR = /usr/local/lib

#-I$(IPERFDIR)

#-I$(IPERFDIR)/build/src -I$(IPERFDIR)/git/src

#-I$(OPTDIR)/src/debug/iperf3/3.1.3+gitrAUTOINC+099244ec68-r0/git/src
# *****************************************************************************
# Add source directories to be compiled here (these don't need a Makefile)
# If we use multiple sub directories for source code we can compile here
# *****************************************************************************

VPATH:= $(SRCDIR) 

# *****************************************************************************
# Add source files to be compiled here
# *****************************************************************************

SRC_FILES := main \
			configParser
             
SRCS := $(SRC_FILES)

# *****************************************************************************
# Build a release and debug version
# *****************************************************************************

OBJDIR_REL:=Object/Release
DEPDIR_REL:=Object/Release

OBJDIR_DBG:=Object/Debug
DEPDIR_DBG:=Object/Debug

# *****************************************************************************
# setup linker flags
# *****************************************************************************

LDFLAGS = -L$(LOCALDIR) -liperf -lpaho-mqtt3c 
LDFLAGS += -L$(ARMDIR) -lpthread

# *****************************************************************************
# setup include paths
# *****************************************************************************

INCLUDEFLAGS += -I$(PROJDIR)/. $(INCDIRS)

# *****************************************************************************
# setup compiler flags and code conditional compilations
# *****************************************************************************

OSFLAG = -DLINUX -D_GNU_SOURCE -D_REENTRANT

CFLAGS = $(OSFLAG) $(CDEFS) $(WARNING) $(INCLUDEFLAGS) -D_THREAD_SAFE

CFLAGS_REL = -Os -Wall -Wno-strict-aliasing 
CFLAGS_DBG = -Wall -Wno-strict-aliasing -g2 -DDEBUG -O0 

# *****************************************************************************
# setup release and debug flags
# *****************************************************************************

REL_OBJS:= $(SRCS:%=$(OBJDIR_REL)/%.o)
REL_DEPS:= $(SRCS:%=$(DEPDIR_REL)/%.d)
DBG_OBJS:= $(SRCS:%=$(OBJDIR_DBG)/%.o)
DBG_DEPS:= $(SRCS:%=$(DEPDIR_DBG)/%.d)
TARGETDIR_REL = Release
TARGETDIR_DBG = Debug

TARGETS:= $(TARGETDIR_REL)/$(APP_NAME) $(TARGETDIR_DBG)/$(APP_NAME)
#TARGETS:= $(TARGETDIR_REL)/$(APP_NAME)


.PHONY: all clean release install debug

all: $(TARGETS)

debug: $(TARGETDIR_DBG)/$(APP_NAME)

release: $(TARGETDIR_REL)/$(APP_NAME)

install:
	mkdir -p $(INSTALL_DIR)
	cp -f $(TARGETDIR_REL)/$(APP_NAME) $(INSTALL_DIR)/.

clean:
	@echo clean
	@rm -rf Object $(TARGETDIR_REL) $(TARGETDIR_DBG)

$(TARGETDIR_REL)/$(APP_NAME): $(REL_OBJS) Makefile
	@echo linking  $@
	@[ -d $(TARGETDIR_REL) ] || mkdir $(TARGETDIR_REL)
	@$(CC) $(REL_OBJS) $(LDFLAGS) -o $@
	@$(STRIP) --strip-unneeded  -R=.comment -R=.note $@

$(TARGETDIR_DBG)/$(APP_NAME): $(DBG_OBJS) Makefile
	@echo linking  $@
	@[ -d $(TARGETDIR_DBG) ] || mkdir $(TARGETDIR_DBG)
	@$(CC) $(DBG_OBJS) $(LDFLAGS) -o $@

$(OBJDIR_REL)/%.o: %.c Makefile
	@[ -d $(DEPDIR_REL) ] || mkdir -p $(DEPDIR_REL)
	@[ -d $(OBJDIR_REL) ] || mkdir -p $(OBJDIR_REL)
	@echo compiling $<
	$(CC) -MMD -MF $(DEPDIR_REL)/$(*F).d -MT $@ $(CFLAGS) $(CFLAGS_REL) -c $< -o $@
	@[ -s $(DEPDIR_REL)/$(*F).d ] || rm -f $(DEPDIR_REL)/$(*F).d

$(OBJDIR_DBG)/%.o: %.c Makefile
	@[ -d $(DEPDIR_DBG) ] || mkdir -p $(DEPDIR_DBG)
	@[ -d $(OBJDIR_DBG) ] || mkdir -p $(OBJDIR_DBG)
	@echo compiling $<
	$(CC) -MMD -MF $(DEPDIR_DBG)/$(*F).d -MT $@ $(CFLAGS) $(CFLAGS_DBG) -c $< -o $@
	@[ -s $(DEPDIR_DBG)/$(*F).d ] || rm -f $(DEPDIR_DBG)/$(*F).d

-include $(REL_DEPS) $(DBG_DEPS)
