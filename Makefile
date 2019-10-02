CXX = g++
CXXFLAGS = -std=c++11 -Wall  -g
LDFLAGS = -lsystemd -lcpprest -lpthread -lboost_system -lssl -lcrypto

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

SRCS = main.cpp status_update.cpp system_info.cpp 
OBJS = $(SRCS:.cpp=.o)
HEADERS = status_update.h system_info.h ltm_exceptions.h

MAIN = ltm-device

all: $(MAIN) $(DEPDIR)


DEPFILES := $(SRCS:%.cpp=$(DEPDIR)/%.d)

$(DEPDIR):
	@mkdir $(DEPDIR)

$(MAIN): $(OBJS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(MAIN) $(LDFLAGS)

%.o : %.cpp $(DEPDIR)/%.d | $(DEPDIR)
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) ltm-device  $(OBJS) $(DEPFILES)

$(DEPFILES):

include $(wildcard $(DEPFILES))
