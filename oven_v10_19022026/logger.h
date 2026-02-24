#ifndef LOGGER_H
#define LOGGER_H

#include "config.h" 

// Initialize SD Card and write CSV header if new file
void initializeLogger();

// Write current metrics to SD Card
void logSystemData();

#endif // LOGGER_H