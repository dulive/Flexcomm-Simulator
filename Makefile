# Makefile wrapper for waf

MODULES=applications,core,csma,ecofen,energy-api,flow-monitor,internet,internet-apps,link-stats,ofswitch13,parser,point-to-point-ethernet,switch-stats,topology,tap-bridge

TOPO=example
CONTROLLER=ns3::SimpleController
CHECKSUM=false
OUTPUTS=outputs
OPTIONS="flexcomm --topo=$(TOPO) --ctrl=$(CONTROLLER) --checksum=$(CHECKSUM)" --cwd=../$(OUTPUTS)
CXXFLAGS="-Wall"

all:
	./ns-3.35/waf -t ns-3.35

run: create_outpu_folder
	./ns-3.35/waf -t ns-3.35 --run $(OPTIONS) 

run_debug: create_outpu_folder
	./ns-3.35/waf -t ns-3.35 --run $(OPTIONS) --gdb 

optimize:
	CXXFLAGS=$(CXXFLAGS) ./ns-3.35/waf -t ns-3.35 -d optimized configure --disable-python --disable-gtk --enable-modules=$(MODULES)

debug:
	CXXFLAGS=$(CXXFLAGS) ./ns-3.35/waf -t ns-3.35 configure --disable-python --disable-gtk --enable-modules=$(MODULES)

format:
	./formatter.sh ns-3.35

create_outpu_folder:
	mkdir -p $(OUTPUTS)/$(TOPO)
