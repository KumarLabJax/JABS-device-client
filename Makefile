PYLONDIR = /opt/pylon5
CXX = g++
CXXFLAGS = -std=c++11 -Wall  -g `$(PYLONDIR)/bin/pylon-config --cflags`
LDFLAGS = `$(PYLONDIR)/bin/pylon-config --libs`
LDLIBS = -lsystemd -lcpprest -lpthread -lboost_system -lssl -lcrypto

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

SRCS = main.cpp status_update.cpp system_info.cpp camera_controller.cpp pylon_camera.cpp video_writer.cpp pixel_types.cpp
OBJS = $(SRCS:.cpp=.o)
HEADERS = status_update.h system_info.h ltm_exceptions.h video_writer.h pixel_types.h

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
