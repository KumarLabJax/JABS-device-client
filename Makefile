PYLON_DIR = /opt/pylon5
FFMPEG_DIR = /opt/ffmpeg-n4.0
INSTALL_DIR = /opt/jax-mba
CXX = g++
CPPFLAGS = -I$(FFMPEG_DIR)/include
CXXFLAGS = -O3 -std=c++11 -Wall  -g `$(PYLON_DIR)/bin/pylon-config --cflags`
LDFLAGS = -L$(FFMPEG_DIR)/lib `$(PYLON_DIR)/bin/pylon-config --libs`
LDLIBS = -lsystemd -lcpprest -lpthread -lboost_system -lssl -lcrypto -lavfilter -lavformat \
  -lavcodec -lswscale  -lswscale -lswresample -lavdevice -lpostproc -lavutil -lz \
  -lx264  -lbz2 -lrt -lX11 -lvdpau -llzma

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

SRCS = main.cpp status_update.cpp system_info.cpp camera_controller.cpp pylon_camera.cpp video_writer.cpp pixel_types.cpp server_command.cpp
OBJS = $(SRCS:.cpp=.o)
HEADERS = status_update.h system_info.h ltm_exceptions.h video_writer.h pixel_types.h camera_controller.h pylon_camera.h server_command.h

MAIN = mba-client

all: $(MAIN) $(DEPDIR)


DEPFILES := $(SRCS:%.cpp=$(DEPDIR)/%.d)

$(DEPDIR):
	@mkdir $(DEPDIR)

$(MAIN): $(OBJS) $(HEADERS)
	LD_LIBRARY_PATH=$(PYLON_DIR)/lib64 $(CXX) $(CXXFLAGS) $(OBJS) -o $(MAIN) $(LDFLAGS)  $(LDLIBS)

%.o : %.cpp $(DEPDIR)/%.d | $(DEPDIR)
	$(CXX) $(CPPFLAGS) $(DEPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(MAIN)  $(OBJS) $(DEPFILES)

install:
	mkdir -p $(INSTALL_DIR)/bin && mkdir $(INSTALL_DIR)/conf && \
	cp $(MAIN) $(INSTALL_DIR)/bin/$(MAIN) && \
	cp systemd/mba-client.service /etc/systemd/system/ && \
	cp conf config_template.ini $(INSTALL_DIR)/conf/jax-mba.ini

$(DEPFILES):

include $(wildcard $(DEPFILES))
