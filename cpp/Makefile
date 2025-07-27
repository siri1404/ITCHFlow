# TickShaper Makefile

CXX = g++
CXXFLAGS = -std=c++17 -O3 -march=native -mtune=native -Wall -Wextra -Wpedantic
CXXFLAGS += -ffast-math -funroll-loops -finline-functions
LDFLAGS = -lzmq -lpthread -lrt

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/tickshaper

# Default target
all: $(TARGET)

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Build target
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Build object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

# Debug build
debug: CXXFLAGS = -std=c++17 -g -O0 -DDEBUG -Wall -Wextra -Wpedantic
debug: $(TARGET)

# Clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Install
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo mkdir -p /etc/tickshaper
	sudo cp config/tickshaper.conf.in /etc/tickshaper/tickshaper.conf

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/tickshaper
	sudo rm -rf /etc/tickshaper

# Dependencies
depend: $(SOURCES)
	$(CXX) -I$(INCDIR) -MM $(SOURCES) > .depend

-include .depend

.PHONY: all debug clean install uninstall depend