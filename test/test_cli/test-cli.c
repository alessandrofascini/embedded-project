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

    TEST_ASSERT_EQUAL(CLI_INIT_SELF_IS_NULL, cli_init(NULL, storage, TEST_STORAGE_SIZE, commands, command_count));
    TEST_ASSERT_EQUAL(CLI_INIT_STORAGE_IS_NULL, cli_init(&cli, NULL, TEST_STORAGE_SIZE, commands, command_count));
    TEST_ASSERT_EQUAL(CLI_INIT_NO_CAPACITY, cli_init(&cli, storage, 0, commands, command_count));
    TEST_ASSERT_EQUAL(CLI_INIT_COMMANDS_IS_NULL, cli_init(&cli, storage, TEST_STORAGE_SIZE, NULL, command_count));

    const struct Command no_grammar[] = { { NULL, prv_command_echo } };
    TEST_ASSERT_EQUAL(CLI_INIT_INVALID_COMMAND, cli_init(&cli, storage, TEST_STORAGE_SIZE, no_grammar, 1));

    const struct Command no_handler[] = { { "echo", NULL } };
    TEST_ASSERT_EQUAL(CLI_INIT_INVALID_COMMAND, cli_init(&cli, storage, TEST_STORAGE_SIZE, no_handler, 1));

    /* A valid init starts from an empty line and asks for a prompt */
    TEST_ASSERT_EQUAL(CLI_INIT_OK, cli_init(&cli, storage, TEST_STORAGE_SIZE, commands, command_count));
    TEST_ASSERT_EQUAL(CLI_FEED_EMPTY, cli.status);
    TEST_ASSERT_EQUAL_STRING("", cli_get_command(&cli));
}

void testCliFeed(void) {
    struct Cli cli;
    char storage[TEST_STORAGE_SIZE];

    cli_init(&cli, storage, TEST_STORAGE_SIZE, commands, command_count);

    TEST_ASSERT_EQUAL(CLI_FEED_SELF_IS_NULL, cli_feed(NULL, 'a'));

    /* Every character updates the line */
    TEST_ASSERT_EQUAL(CLI_FEED_UPDATED, cli_feed(&cli, 'e'));
    TEST_ASSERT_EQUAL(CLI_FEED_UPDATED, cli_feed(&cli, 'c'));
    TEST_ASSERT_EQUAL_STRING("ec", cli_get_command(&cli));

    /* Backspace deletes the last character */
    TEST_ASSERT_EQUAL(CLI_FEED_UPDATED, cli_feed(&cli, '\b'));
    TEST_ASSERT_EQUAL_STRING("e", cli_get_command(&cli));

    /* Enter makes the line executable */
    TEST_ASSERT_EQUAL(CLI_FEED_EXECUTABLE, cli_feed(&cli, '\r'));

    /* While the line is executable, every byte is dropped */
    TEST_ASSERT_EQUAL(CLI_FEED_EXECUTABLE, cli_feed(&cli, 'x'));
    TEST_ASSERT_EQUAL(CLI_FEED_EXECUTABLE, cli_feed(&cli, '\b'));
    TEST_ASSERT_EQUAL_STRING("e\r", cli_get_command(&cli));

    /* LF is a terminator as well */
    cli_init(&cli, storage, TEST_STORAGE_SIZE, commands, command_count);
    TEST_ASSERT_EQUAL(CLI_FEED_UPDATED, cli_feed(&cli, 'e'));
    TEST_ASSERT_EQUAL(CLI_FEED_EXECUTABLE, cli_feed(&cli, '\n'));

    /* Backspace on an empty line changes nothing, but still reports an update */
    cli_init(&cli, storage, TEST_STORAGE_SIZE, commands, command_count);
    TEST_ASSERT_EQUAL(CLI_FEED_UPDATED, cli_feed(&cli, '\b'));
    TEST_ASSERT_EQUAL_STRING("", cli_get_command(&cli));

    /* A full line drops every further byte, Enter included */
    for (int i = 0; i < TEST_STORAGE_SIZE - 1; i++) {
        TEST_ASSERT_EQUAL(CLI_FEED_UPDATED, cli_feed(&cli, 'a'));
    }
    TEST_ASSERT_EQUAL(CLI_FEED_NOT_CHANGED, cli_feed(&cli, 'b'));
    TEST_ASSERT_EQUAL(CLI_FEED_NOT_CHANGED, cli_feed(&cli, '\r'));
    TEST_ASSERT_EQUAL_STRING("aaaaaaaaaaaaaaa", cli_get_command(&cli));

    /* ...until a backspace frees a slot */
    TEST_ASSERT_EQUAL(CLI_FEED_UPDATED, cli_feed(&cli, '\b'));
    TEST_ASSERT_EQUAL(CLI_FEED_UPDATED, cli_feed(&cli, 'b'));
    TEST_ASSERT_EQUAL_STRING("aaaaaaaaaaaaaab", cli_get_command(&cli));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testCliInit);
    RUN_TEST(testCliFeed);
    return UNITY_END();
}
