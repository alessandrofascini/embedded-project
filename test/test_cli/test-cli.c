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
#include "fff.h"

#include <string.h>

DEFINE_FFF_GLOBALS;
FAKE_VALUE_FUNC(enum CommandHandlerStatus, command_echo, int, char **);

#define TEST_STORAGE_SIZE (16) /* longest line: 15 characters */

static const struct Command commands[] = {
    { "echo", command_echo },
};
static const size_t command_count = sizeof(commands) / sizeof(commands[0]);

static char echo_argv[COMMAND_EXECUTOR_MAX_ARGS][TEST_STORAGE_SIZE];

static enum CommandHandlerStatus prv_copy_args(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        strncpy(echo_argv[i], argv[i], TEST_STORAGE_SIZE - 1);
    }
    return COMMAND_HANDLER_OK;
}

/*!
 * \brief Feed a whole string to the CLI, one byte at a time.
 * \param cli  The Cli instance.
 * \param text The bytes to feed.
 */
static void prv_feed_string(struct Cli *cli, const char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        cli_feed(cli, text[i]);
    }
}

void setUp(void) {
    RESET_FAKE(command_echo);
    FFF_RESET_HISTORY();
    memset(echo_argv, 0, sizeof(echo_argv));
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

    const struct Command no_grammar[] = { { NULL, command_echo } };
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

void testCliExecute(void) {
    command_echo_fake.custom_fake = prv_copy_args;

    struct Cli cli;
    char storage[TEST_STORAGE_SIZE];

    cli_init(&cli, storage, TEST_STORAGE_SIZE, commands, command_count);

    TEST_ASSERT_EQUAL(COMMAND_EXECUTOR_SELF_IS_NULL, cli_execute(NULL));
    TEST_ASSERT_EQUAL(COMMAND_EXECUTOR_LINE_NOT_READY, cli_execute(&cli));

    /* Without Enter the line is not ready: refused, and kept as it is */

    prv_feed_string(&cli, "echo a bc");
    TEST_ASSERT_EQUAL(COMMAND_EXECUTOR_LINE_NOT_READY, cli_execute(&cli));
    TEST_ASSERT_EQUAL_STRING("echo a bc", cli_get_command(&cli));
    TEST_ASSERT_EQUAL(0, command_echo_fake.call_count);

    cli_feed(&cli, '\r');
    TEST_ASSERT_EQUAL(COMMAND_EXECUTOR_OK, cli_execute(&cli));
    TEST_ASSERT_EQUAL(1, command_echo_fake.call_count);
    TEST_ASSERT_EQUAL(3, command_echo_fake.arg0_val);
    TEST_ASSERT_EQUAL_STRING("echo", echo_argv[0]);
    TEST_ASSERT_EQUAL_STRING("a", echo_argv[1]);
    TEST_ASSERT_EQUAL_STRING("bc", echo_argv[2]);

    TEST_ASSERT_EQUAL(CLI_FEED_EMPTY, cli.status);
    TEST_ASSERT_EQUAL_STRING("", cli_get_command(&cli));

    /* Leading, repeated and mixed spaces and tabs are all skipped */
    memset(echo_argv, 0, sizeof(echo_argv));
    prv_feed_string(&cli, " \techo \t a  \r");
    TEST_ASSERT_EQUAL(COMMAND_EXECUTOR_OK, cli_execute(&cli));
    TEST_ASSERT_EQUAL(2, command_echo_fake.call_count);
    TEST_ASSERT_EQUAL(2, command_echo_fake.arg0_val);
    TEST_ASSERT_EQUAL_STRING("echo", echo_argv[0]);
    TEST_ASSERT_EQUAL_STRING("a", echo_argv[1]);

    prv_feed_string(&cli, "echo a b c\r");
    TEST_ASSERT_EQUAL(COMMAND_EXECUTOR_OK, cli_execute(&cli));
    TEST_ASSERT_EQUAL(COMMAND_EXECUTOR_MAX_ARGS, command_echo_fake.arg0_val);

    memset(echo_argv, 0, sizeof(echo_argv));
    prv_feed_string(&cli, "echo a b c d\r");
    TEST_ASSERT_EQUAL(COMMAND_EXECUTOR_TOO_MANY_ARGS, cli_execute(&cli));
    TEST_ASSERT_EQUAL(3, command_echo_fake.call_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testCliInit);
    RUN_TEST(testCliFeed);
    RUN_TEST(testCliExecute);
    return UNITY_END();
}
