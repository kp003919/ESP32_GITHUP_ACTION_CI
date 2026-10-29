/**
 * TestCommands.h - Serial-based diagnostic and CI/CD test commands
 */
#ifndef TEST_COMMANDS_H
#define TEST_COMMANDS_H

// Call this from your main loop() — it reads Serial and runs matching tests
void handle_serial();

#endif // TEST_COMMANDS_H