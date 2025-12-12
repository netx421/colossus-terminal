CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

PKG      := gtk+-3.0 vte-2.91
INCLUDES := $(shell pkg-config --cflags $(PKG))
LIBS     := $(shell pkg-config --libs $(PKG))

TARGET   := Terminal
SRC      := main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)
