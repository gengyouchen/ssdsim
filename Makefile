systemc_header = "/usr/local/Cellar/systemc/2.3.2/include"
systemc_library = "/usr/local/Cellar/systemc/2.3.2/lib"

CXXFLAGS += -I$(systemc_header)
LDFLAGS += -L$(systemc_library)
LIBS += -lsystemc

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBS) -o run.x gyc_ftl_p_meta_table.cpp gyc_ftl_wr_ptr.cpp simple_die_ftl.cpp simple_die_scheduler.cpp simple_die_controller.cpp simple_ssd_controller.cpp simple_io_scheduler.cpp main.cpp

clean:
	rm -f run.x

run:
	./run.x trace/example.trace 4 0

