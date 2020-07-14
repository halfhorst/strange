CCFLAGS = -Wall -pedantic
LDFLAGS = -lm

TARGET = strangeland
OBJECTS = cli.o tty.o renderer.o demos/denabase.o demos/digital_rain.o

all: strangeland

debug: CCFLAGS += -g
debug: strangeland

benchmark: CCFLAGS += -DDEBUG -pg
benchmark: strangeland

strangeland: $(OBJECTS)
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@ $(LDFLAGS)

demos/%.o: demos/%.c
	$(CC) $(CCFLAGS) -c $< -o $@ $(LDFLAGS)

clean:
	rm -f *.o demos/*.o strangeland

denabase: strangeland
	./strangeland  denabase

cube: strangeland
	./strangeland --delay=250 cube

digital_rain: strangeland
	./strangeland digital_rain
