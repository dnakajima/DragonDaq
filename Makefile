TARGET = DragonDaqMOnlineCarlos
DEP=dep.d
CXX = g++
all: dep $(TARGET)

$(TARGET): % : $(addsuffix .cpp, $(basename $(TARGET)))
	$(CXX) `root-config --cflags  --libs` -o $@ $< -lrt

DragonDaqMOnline.o:DragonDaqOnlineCarlos.cpp
	$(CXX) `root-config --cflags  --libs` -c %<
dep:
	g++ `root-config --cflags` -MM DragonDaqMOnlineCarlos.cpp > $(DEP)

-include $(DEP)

clean:
	rm DragonDaqMOnlineCarlos
#DragonDaqM:
#	g++ -o DragonDaqM DragonDaqM.cpp -lrt
#DragonDaqMOnline:
#	g++ -o DragonDaqMOnline DragonDaqMOnline.cpp -lrt
