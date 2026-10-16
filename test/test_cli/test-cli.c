/*!
 * \file test-cli.c
 * \author Alessandro Fascini
 *
 * \brief Cli test file.
 *
 * The CLI is tested only through its public API (cli-api.h): the command
 * buffer and the command executor are exercised as its internals.
 */

#include "unity.h"
#include "cli-api.h"

#define TEST_STORAGE_SIZE (16) /* longest line: 15 characters */

/*!
 * \brief Handler that always accepts its arguments.
 * \param argc Unused.
 * \param argv Unused.
 * \return COMMAND_HANDLER_OK.
 */
static enum CommandHandlerStatus prv_command_echo(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    return COMMAND_HANDLER_OK;
}

static const struct Command commands[] = {
    { "echo", prv_command_echo },
};
static const size_t command_count = sizeof(commands) / sizeof(commands[0]);

void setUp(void) {
}

void tearDown(void) {
}

void testCliInit(void) {
    struct Cli cli;
    char storage[TEST_STORAGE_SIZE];

    TEST_ASSERT_EQUAL(CLI_INIT_SELF_IS_NULL, cli_init(NULL, storage, sizeof(storage), commands, command_count));
    TEST_ASSERT_EQUAL(CLI_INIT_STORAGE_IS_NULL, cli_init(&cli, NULL, sizeof(storage), commands, command_count));
    TEST_ASSERT_EQUAL(CLI_INIT_NO_CAPACITY, cli_init(&cli, storage, 0, commands, command_count));
    TEST_ASSERT_EQUAL(CLI_INIT_COMMANDS_IS_NULL, cli_init(&cli, storage, sizeof(storage), NULL, command_count));

    const struct Command no_grammar[] = { { NULL, prv_command_echo } };
    TEST_ASSERT_EQUAL(CLI_INIT_INVALID_COMMAND, cli_init(&cli, storage, sizeof(storage), no_grammar, 1));

    const struct Command no_handler[] = { { "echo", NULL } };
    TEST_ASSERT_EQUAL(CLI_INIT_INVALID_COMMAND, cli_init(&cli, storage, sizeof(storage), no_handler, 1));

    /* A valid init starts from an empty line and asks for a prompt */
    TEST_ASSERT_EQUAL(CLI_INIT_OK, cli_init(&cli, storage, sizeof(storage), commands, command_count));
    TEST_ASSERT_EQUAL(CLI_FEED_EMPTY, cli.status);
    TEST_ASSERT_EQUAL_STRING("", cli_get_command(&cli));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testCliInit);
    return UNITY_END();
}
