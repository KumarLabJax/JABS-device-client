CXX = g++
CXXFLAGS = -std=c++11 -Wall  -g
LDFLAGS = -lsystemd -lcpprest -lboost_system -lssl -l crypto

SRCS = main.cpp status_update.cpp system_info.cpp 
OBJS = ${SRCS:.cpp=.o}
HEADERS = status_update.h system_info.h

MAIN = ltm-device

all: ${MAIN}

${MAIN}: ${OBJS} ${HEADERS}
	${CXX} ${CXXFLAGS} ${OBJS} -o ${MAIN} ${LDFLAGS}

.cpp.o:
	${CXX} ${CXXFLAGS} -c $< -o $@

clean:
	${RM} ${PROGS} ${OBJS} *.o *~
