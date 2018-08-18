/* cl65 -I. -t sim6502 -o test -m test.map json65.s test.c */

#include <stdio.h>
#include <string.h>
#include <json65.h>

static j65_parser state;
static int passes, failures;

#define MAGIC 0x2badbeef

typedef struct {
    int8_t ev;
    int32_t integer;
    const char *str;
    uint32_t line_no;
} event_check;

typedef struct {
    uint32_t magic;
    const event_check *events;
    size_t len;
    size_t pos;
} my_context;

static const event_check test00[] = {
  { J65_DONE, 0, "[] ", 0 },
  { J65_START_ARRAY,       0, NULL,             0 },
  { J65_END_ARRAY,         0, NULL,             0 },
};

static const event_check test01[] = {
  { J65_DONE, 1, "{} ", 0 },
  { J65_START_OBJ,         0, NULL,             0 },
  { J65_END_OBJ,           0, NULL,             0 },
};

static const event_check test02[] = {
  { J65_DONE, 2, "1234 ", 0 },
  { J65_INTEGER,        1234, "1234",           0 },
};

static const event_check test03[] = {
  { J65_DONE, 3, "-10000000 ", 0 },
  { J65_INTEGER,   -10000000, "-10000000",      0 },
};

static const event_check test04[] = {
  { J65_DONE, 4, "1.5 ", 0 },
  { J65_NUMBER,            0, "1.5",            0 },
};

static const event_check test05[] = {
  { J65_DONE, 5, "1e-2 ", 0 },
  { J65_NUMBER,            0, "1e-2",           0 },
};

static const event_check test06[] = {
  { J65_DONE, 6, "\"Hello, World\" ", 0 },
  { J65_STRING,            0, "Hello, World",   0 },
};

static const event_check test07[] = {
  { J65_DONE, 7, "null ", 0 },
  { J65_NULL,              0, NULL,             0 },
};

static const event_check test08[] = {
  { J65_DONE, 8, "false ", 0 },
  { J65_FALSE,             0, NULL,             0 },
};

static const event_check test09[] = {
  { J65_DONE, 9, "true ", 0 },
  { J65_TRUE,              0, NULL,             0 },
};

static const event_check test10[] = {
  { J65_DONE, 10, "{\"foo\": 5} ", 0 },
  { J65_START_OBJ,         0, NULL,             0 },
  { J65_KEY,               0, "foo",            0 },
  { J65_INTEGER,           5, "5",              0 },
  { J65_END_OBJ,           0, NULL,             0 },
};

static const event_check test11[] = {
  { J65_DONE, 11, "{\"foo\": \"bar\", \"baz\": [1, 2, 3]} ", 0 },
  { J65_START_OBJ,         0, NULL,             0 },
  { J65_KEY,               0, "foo",            0 },
  { J65_STRING,            0, "bar",            0 },
  { J65_KEY,               0, "baz",            0 },
  { J65_START_ARRAY,       0, NULL,             0 },
  { J65_INTEGER,           1, "1",              0 },
  { J65_INTEGER,           2, "2",              0 },
  { J65_INTEGER,           3, "3",              0 },
  { J65_END_ARRAY,         0, NULL,             0 },
  { J65_END_OBJ,           0, NULL,             0 },
};

static const event_check test12[] = {
    { J65_WANT_MORE, 12, "[1, 2, 3", 0 },
    { J65_START_ARRAY,     0, NULL,             0 },
    { J65_INTEGER,         1, "1",              0 },
    { J65_INTEGER,         2, "2",              0 },
};

static const event_check test13[] = {
    { J65_DONE, 13, "\n\"slash \\/ tab \\t\"", 1 },
    { J65_STRING,          0, "slash / tab \t", 1 },
};

static const event_check test14[] = {
    { J65_DONE, 14, "\"slash \\/ backslash \\\\ tab \\t\"", 0 },
    { J65_STRING,          0, "slash / backslash \\ tab \t", 0 },
};

static const event_check test15[] = {
    { J65_DONE, 15, "\"have \\u0061 nice day\"", 0 },
    { J65_STRING,          0, "have a nice day", 0 },
};

static const event_check test16[] = {
    { J65_DONE, 16, "\"have \\u0061\\u0020nice day\"", 0 },
    { J65_STRING,          0, "have a nice day", 0 },
};

static const event_check test17[] = {
    { J65_DONE, 17, "\"have \\u0061\\\\nice day\"", 0 },
    { J65_STRING,          0, "have a\\nice day", 0 },
};

static const event_check test18[] = {
    { J65_DONE, 18, "\"this \\uD834\\uDD1E is a G clef\"", 0 },
    { J65_STRING,          0, "this 𝄞 is a G clef", 0 },
};

static const event_check test19[] = {
    { J65_DONE, 19, "\"\\u00a9 2018\"", 0 },
    { J65_STRING,          0, "© 2018", 0 },
};

static const event_check test20[] = {
    { J65_DONE, 20, "\"cents \\u00a2 Euros \\u20ac\"", 0 },
    { J65_STRING,          0, "cents ¢ Euros €", 0 },
};

static const event_check test21[] = {
    { J65_DONE, 21, "[\nnull\n,\nfalse\n,\ntrue\n]\n", 6 },
    { J65_START_ARRAY,     0, NULL,              0 },
    { J65_NULL,            0, NULL,              1 },
    { J65_FALSE,           0, NULL,              3 },
    { J65_TRUE,            0, NULL,              5 },
    { J65_END_ARRAY,       0, NULL,              6 },
};

static const event_check test22[] = {
    { J65_EXPECTED_STRING, 22, "{false: true} ", 0 },
    { J65_START_OBJ,       0, NULL,              0 },
};

static const event_check test23[] = {
    { J65_EXPECTED_COLON, 23, "{\"hello\", \"world\"} ", 0 },
    { J65_START_OBJ,       0, NULL,              0 },
    { J65_KEY,             0, "hello",           0 },
};

static const event_check test24[] = {
    { J65_DONE, 24, "[\rnull\r,\rfalse\r,\rtrue\r]\r", 6 },
    { J65_START_ARRAY,     0, NULL,              0 },
    { J65_NULL,            0, NULL,              1 },
    { J65_FALSE,           0, NULL,              3 },
    { J65_TRUE,            0, NULL,              5 },
    { J65_END_ARRAY,       0, NULL,              6 },
};

static const event_check test25[] = {
    { J65_DONE, 25, "[\r\nnull\r\n,\r\nfalse\r\n,\r\ntrue\r\n]\r\n", 6 },
    { J65_START_ARRAY,     0, NULL,              0 },
    { J65_NULL,            0, NULL,              1 },
    { J65_FALSE,           0, NULL,              3 },
    { J65_TRUE,            0, NULL,              5 },
    { J65_END_ARRAY,       0, NULL,              6 },
};

static const event_check test26[] = {
  { J65_DONE, 26, "2147483647 ", 0 },
  { J65_INTEGER,  2147483647, "2147483647",           0 },
};

static const event_check test27[] = {
  { J65_DONE, 27, "-2147483647 ", 0 },
  { J65_INTEGER, -2147483647, "-2147483647",          0 },
};

static const event_check test28[] = {
  { J65_DONE, 28, "2147483648 ", 0 },
  { J65_NUMBER,            0, "2147483648",           0 },
};

static const event_check test29[] = {
  { J65_DONE, 29, "-2147483648 ", 0 },
  { J65_INTEGER, -2147483648, "-2147483648",          0 },
};

static const event_check test30[] = {
  { J65_DONE, 30, "-2147483649 ", 0 },
  { J65_NUMBER,            0, "-2147483649",          0 },
};

static const event_check test31[] = {
  { J65_DONE, 31, "-4294967296 ", 0 },
  { J65_NUMBER,            0, "-4294967296",          0 },
};

static const event_check test32[] = {
  { J65_DONE, 32, "4294967296 ", 0 },
  { J65_NUMBER,            0, "4294967296",           0 },
};

static const event_check test33[] = {
  { J65_DONE, 33, "10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 ", 0 },
  { J65_NUMBER, 0, "10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 0 },
};

static const event_check test34[] = {
  { J65_DONE, 34, "-10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 ", 0 },
  { J65_NUMBER, 0, "-10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 0 },
};

static const event_check test35[] = {
  { J65_STRING_TOO_LONG, 35, "10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 ", 0 },
};

static const event_check test36[] = {
  { J65_PARSE_ERROR, 36, "5-5 ", 0 },
};

static const event_check test37[] = {
  { J65_ILLEGAL_CHAR, 37, "0x2000 ", 0 },
  { J65_INTEGER,       0, "0",       0 },
};

static const event_check test38[] = {
  { J65_ILLEGAL_CHAR, 38, "barf ", 0 },
};

static const event_check test39[] = {
  { J65_PARSE_ERROR,  39, "nue ", 0 },
};

static const event_check test40[] = {
    { J65_PARSE_ERROR, 40, "]", 0 },
};

static const event_check test41[] = {
    { J65_ILLEGAL_ESCAPE, 41, "\"This is \\j not allowed\"", 0 },
};

static const event_check test42[] = {
    { J65_ILLEGAL_ESCAPE, 42, "\"This is \\uucp not either\"", 0 },
};

static const event_check test43[] = {
    { J65_ILLEGAL_ESCAPE, 43, "\"And this? \\u\"", 0 },
};

static const event_check test44[] = {
    { J65_DONE, 44, "{\"foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo\": \"baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaar\"}", 0 },
    { J65_START_OBJ,         0, NULL,             0 },
    { J65_KEY,               0, "foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo", 0 },
    { J65_STRING,            0, "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaar", 0 },
    { J65_END_OBJ,           0, NULL,             0 },
};

static const event_check test45[] = {
    { J65_DONE, 45, "[[[[]]]]", 0 },
    { J65_START_ARRAY,       0, NULL,             0 },
    { J65_START_ARRAY,       0, NULL,             0 },
    { J65_START_ARRAY,       0, NULL,             0 },
    { J65_START_ARRAY,       0, NULL,             0 },
    { J65_END_ARRAY,         0, NULL,             0 },
    { J65_END_ARRAY,         0, NULL,             0 },
    { J65_END_ARRAY,         0, NULL,             0 },
    { J65_END_ARRAY,         0, NULL,             0 },
};

static const event_check test46[] = {
    { J65_NESTING_TOO_DEEP, 46, "[[[[[]]]]]", 0 },
    { J65_START_ARRAY,       0, NULL,             0 },
    { J65_START_ARRAY,       0, NULL,             0 },
    { J65_START_ARRAY,       0, NULL,             0 },
    { J65_START_ARRAY,       0, NULL,             0 },
};

static const char *event_name (uint8_t event) {
    switch (event) {
    case J65_NULL        : return "J65_NULL";
    case J65_FALSE       : return "J65_FALSE";
    case J65_TRUE        : return "J65_TRUE";
    case J65_INTEGER     : return "J65_INTEGER";
    case J65_NUMBER      : return "J65_NUMBER";
    case J65_STRING      : return "J65_STRING";
    case J65_KEY         : return "J65_KEY";
    case J65_START_OBJ   : return "J65_START_OBJ";
    case J65_END_OBJ     : return "J65_END_OBJ";
    case J65_START_ARRAY : return "J65_START_ARRAY";
    case J65_END_ARRAY   : return "J65_END_ARRAY";
    default: return "?";
    }
}

static void print_pass (void) {
    printf ("\033[32mPASS\033[0m\n");
    passes++;
}

static void print_fail (void) {
    printf ("\033[31mFAIL\033[0m\n");
    failures++;
}

static int8_t callback (j65_parser *p, uint8_t event) {
    my_context *ctx = (my_context *) j65_get_context(p);
    const char *ename = event_name (event);
    size_t pos = ctx->pos;
    const event_check *ec;
    int32_t i;
    const char *str;
    size_t len1, len2;
    uint32_t line_no;

    if (ctx->magic != MAGIC) {
        print_fail();
        printf ("Got magic $%08lx but expected $%08lx\n", ctx->magic, MAGIC);
        return J65_USER_ERROR;
    }

    if (pos >= ctx->len) {
        print_fail();
        printf ("Got extra event of type %s\n", ename);
        return J65_USER_ERROR;
    }

    ec = ctx->events + pos;

    if (ec->ev != event) {
        print_fail();
        printf ("[%u] Got %s but expected %s\n",
                pos, ename, event_name(ec->ev));
        return J65_USER_ERROR;
    }

    if (event == J65_INTEGER) {
        i = j65_get_integer(p);
        if (i != ec->integer) {
            print_fail();
            printf ("[%u] Got %ld but expected %ld\n", pos, i, ec->integer);
            return J65_USER_ERROR;
        }
    }

    if (event == J65_INTEGER || event == J65_NUMBER ||
        event == J65_STRING  || event == J65_KEY) {
        str = j65_get_string(p);
        len1 = j65_get_length(p);
        len2 = strlen (str);

        if (len1 != len2) {
            print_fail();
            printf ("[%u] String length is %u but claimed to be %u\n",
                    pos, len2, len1);
            return J65_USER_ERROR;
        }

        if (strcmp (str, ec->str) != 0) {
            print_fail();
            printf ("[%u] For %s, got '%s' but expected '%s'\n",
                    pos, ename, str, ec->str);
            return J65_USER_ERROR;
        }
    }

    line_no = j65_get_line_number(p);
    if (line_no != ec->line_no) {
        print_fail();
        printf ("[%u] For %s, got line %lu but expected %lu\n",
                pos, ename, line_no, ec->line_no);
        return J65_USER_ERROR;
    }

    ctx->pos++;

    return 0;
}

static void run_test (const event_check *events, size_t len) {
    my_context ctx;
    uint32_t line_no;
    int8_t ret;
    const char *str = events->str;

    printf ("test %02ld: ", events->integer);

    ctx.magic = MAGIC;
    ctx.events = events;
    ctx.len = len;
    ctx.pos = 1;

    /* Use a small nesting depth to make it easy to test. */
    j65_init (&state, (void *) &ctx, callback, 4);
    ret = j65_parse(&state, str, strlen(str));

    if (ret == J65_USER_ERROR) {
        return;
    }

    if (ret != events->ev) {
        print_fail();
        printf ("Got return code %d but expected %d\n", ret, events->ev);
        return;
    }

    if (ctx.pos != ctx.len) {
        print_fail();
        printf ("Got %u events but expected %u\n", ctx.pos - 1, ctx.len - 1);
        return;
    }

    line_no = j65_get_line_number(&state);
    if (line_no != events->line_no) {
        print_fail();
        printf ("Final line number was %lu but expected %lu\n",
                line_no, events->line_no);
        return;
    }

    print_pass();
}

#define TEST(x) run_test (x, sizeof(x) / sizeof(x[0]))

int main (int argc, char **argv) {
    int color;

    TEST(test00);
    TEST(test01);
    TEST(test02);
    TEST(test03);
    TEST(test04);
    TEST(test05);
    TEST(test06);
    TEST(test07);
    TEST(test08);
    TEST(test09);
    TEST(test10);
    TEST(test11);
    TEST(test12);
    TEST(test13);
    TEST(test14);
    TEST(test15);
    TEST(test16);
    TEST(test17);
    TEST(test18);
    TEST(test19);
    TEST(test20);
    TEST(test21);
    TEST(test22);
    TEST(test23);
    TEST(test24);
    TEST(test25);
    TEST(test26);
    TEST(test27);
    TEST(test28);
    TEST(test29);
    TEST(test30);
    TEST(test31);
    TEST(test32);
    TEST(test33);
    TEST(test34);
    TEST(test35);
    TEST(test36);
    TEST(test37);
    TEST(test38);
    TEST(test39);
    TEST(test40);
    TEST(test41);
    TEST(test42);
    TEST(test43);
    TEST(test44);
    TEST(test45);
    TEST(test46);

    if (failures > 0)
        color = 31;             /* red */
    else
        color = 32;             /* green */

    printf ("\033[%dm================================\033[0m\n", color);
    printf ("%d tests passed; %d tests failed\n", passes, failures);
    printf ("\033[%dm================================\033[0m\n", color);

    return failures;
}
