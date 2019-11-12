PYLONDIR = /opt/pylon5
CXX = g++
CPPFLAGS = -I/opt/ffmpeg-n4.0/include
CXXFLAGS = -O3 -std=c++11 -Wall  -g `$(PYLONDIR)/bin/pylon-config --cflags`
LDFLAGS = -L/opt/ffmpeg-n4.0/lib `$(PYLONDIR)/bin/pylon-config --libs`
LDLIBS = -lsystemd -lcpprest -lpthread -lboost_system -lssl -lcrypto -lavfilter -lavformat -lavcodec -lswscale  -lswscale -lswresample -lavdevice -lpostproc -lavutil -lz -lx264  -lbz2 -lrt -lX11 -lvdpau -llzma

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

SRCS = main.cpp status_update.cpp system_info.cpp camera_controller.cpp pylon_camera.cpp video_writer.cpp pixel_types.cpp server_command.cpp
OBJS = $(SRCS:.cpp=.o)
HEADERS = status_update.h system_info.h ltm_exceptions.h video_writer.h pixel_types.h camera_controller.h pylon_camera.h server_command.h

MAIN = ltm-device

all: $(MAIN) $(DEPDIR)


DEPFILES := $(SRCS:%.cpp=$(DEPDIR)/%.d)

$(DEPDIR):
	@mkdir $(DEPDIR)

$(MAIN): $(OBJS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(MAIN) $(LDFLAGS)  $(LDLIBS)

%.o : %.cpp $(DEPDIR)/%.d | $(DEPDIR)
	$(CXX) $(CPPFLAGS) $(DEPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) ltm-device  $(OBJS) $(DEPFILES)

$(DEPFILES):

include $(wildcard $(DEPFILES))
