CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -std=c11
SOURCES = process_manager.c thread_manager.c ipc_module.c main_sim.c
TARGET  = eduos

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) -lrt

race:
	$(CC) -Wall -Wextra -pthread -std=c11 -DRACE_CONDITION \
	-o eduos_race $(SOURCES) -lrt
	@echo "Running race condition demo 5 times:"
	@for i in 1 2 3 4 5; do ./eduos_race 2>/dev/null | grep "RACE"; done

fixed:
	$(CC) $(CFLAGS) -DMUTEX_FIXED \
	-o eduos_fixed $(SOURCES) -lrt
	@echo "Running mutex fixed demo:"
	@./eduos_fixed 2>/dev/null | grep "FIXED"

memcheck:
	valgrind --leak-check=full --track-origins=yes ./$(TARGET)

clean:
	rm -f $(TARGET) eduos_race eduos_fixed *.o pcb_snapshot.json
