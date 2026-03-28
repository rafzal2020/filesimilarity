#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>

#define READ_BUF_SIZE 4096
#define WORD_INIT_CAP 16
#define FILES_INIT_CAP 16
#define DEFAULT_SUFFIX ".txt"

typedef struct WordNode
{
    char *word;
    int count;
    struct WordNode *next;
} WordNode;

typedef struct
{
    char *path;    /* path as given / discovered            */
    WordNode *wfd; /* sorted linked list of (word,freq)     */
    int total;     /* total word count                      */
} FileRecord;

/* ── comparison result ───────────────────────────────────────── */
typedef struct
{
    int idx1, idx2;
    int combined;
    double jsd;
} Comparison;

/* ── global file list ────────────────────────────────────────── */
static FileRecord *g_files = NULL;
static int g_nfiles = 0;
static int g_cap = 0;

/* suffix to filter directory entries (default ".txt") */
static const char *g_suffix = DEFAULT_SUFFIX;

static WordNode *wfd_insert(WordNode *head, const char *word)
{
    WordNode *prev = NULL, *cur = head;

    while (cur && strcmp(cur->word, word) < 0)
    {
        prev = cur;
        cur = cur->next;
    }

    if (cur && strcmp(cur->word, word) == 0)
    {
        cur->count++;
        return head;
    }

    /* new node */
    WordNode *n = malloc(sizeof(WordNode));
    if (!n)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    n->word = strdup(word);
    if (!n->word)
    {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    n->count = 1;
    n->next = cur;

    if (prev)
    {
        prev->next = n;
        return head;
    }
    return n; /* new head */
}

static void wfd_free(WordNode *head)
{
    while (head)
    {
        WordNode *tmp = head->next;
        free(head->word);
        free(head);
        head = tmp;
    }
}

static int is_word_char(unsigned char c)
{
    return isalpha(c) || isdigit(c) || c == '-';
}

static FileRecord parse_file(const char *path)
{
    FileRecord fr;
    fr.path = strdup(path);
    fr.wfd = NULL;
    fr.total = 0;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        perror(path);
        return fr;
    }

    /* dynamic word buffer */
    int wbuf_cap = WORD_INIT_CAP;
    char *wbuf = malloc(wbuf_cap);
    if (!wbuf)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    int wlen = 0; /* current word length (0 = not in word) */
    int in_word = 0;

    char buf[READ_BUF_SIZE];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        for (ssize_t i = 0; i < n; i++)
        {
            unsigned char c = (unsigned char)buf[i];

            if (is_word_char(c))
            {
                /* grow buffer if needed (leave room for '\0') */
                if (wlen + 1 >= wbuf_cap)
                {
                    wbuf_cap *= 2;
                    wbuf = realloc(wbuf, wbuf_cap);
                    if (!wbuf)
                    {
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                }
                wbuf[wlen++] = (char)tolower(c);
                in_word = 1;
            }
            else
            {
                /* non-word character: if we were in a word, emit it */
                if (in_word)
                {
                    wbuf[wlen] = '\0';
                    fr.wfd = wfd_insert(fr.wfd, wbuf);
                    fr.total++;
                    wlen = 0;
                    in_word = 0;
                }
                /* ignore the non-word, non-whitespace char */
            }
        }
    }

    if (n < 0)
        perror(path);

    /* flush last word if file didn't end with whitespace */
    if (in_word)
    {
        wbuf[wlen] = '\0';
        fr.wfd = wfd_insert(fr.wfd, wbuf);
        fr.total++;
    }

    free(wbuf);
    close(fd);
    return fr;
}

static void add_file(const char *path)
{
    if (g_nfiles == g_cap)
    {
        g_cap = g_cap ? g_cap * 2 : FILES_INIT_CAP;
        g_files = realloc(g_files, g_cap * sizeof(FileRecord));
        if (!g_files)
        {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
    g_files[g_nfiles++] = parse_file(path);
}

static double find_JSD(const FileRecord *File1, const FileRecord *File2)
{

    // ptrs to iterate through word freq lists
    WordNode *f1 = File1->wfd;
    WordNode *f2 = File2->wfd;

    // totals for KLD
    double kld1 = 0.0;
    double kld2 = 0.0;

    // iterate through lists
    while (f1 || f2)
    {
        const char *word;                // current word
        double freq1 = 0.0, freq2 = 0.0; // frequencies for I and J

        // word exists in both lists:
        if (f1 && f2 && strcmp(f1->word, f2->word) == 0)
        {
            freq1 = (double)f1->count / File1->total;
            freq2 = (double)f2->count / File2->total;
            word = f1->word;

            f1 = f1->next;
            f2 = f2->next;
        }

        // word only exists in file 1
        else if (f2 == NULL || (f1 && strcmp(f1->word, f2->word) < 0))
        {
            freq1 = (double)f1->count / File1->total;
            freq2 = 0.0;
            word = f1->word;
            f1 = f1->next;
        }

        // word only exists in file 2
        else if (f1 == NULL || (f2 && strcmp(f1->word, f2->word) < 0))
        {
            freq1 = 0.0;
            freq2 = (double)f2->count / File2->total;
            word = f2->word;
            f2 = f2->next;
        }

        // find mean probability for the word
        double mean = (freq1 + freq2) / 2;

        // update KLD for word 1 if freq1 > 0
        if (freq1 > 0)
            kld1 += freq1 * log2(freq1 / mean);

        // update KLD for word 2 if freq2 > 0
        if (freq2 > 0)
            kld2 += freq2 * log2(freq2 / mean);

        return sqrt((kld1 + kld2) / 2.0);
    }
}

// temporary test main from chat
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s file1 file2\n", argv[0]);
        return 1;
    }

    FileRecord A = parse_file(argv[1]);
    FileRecord B = parse_file(argv[2]);

    double jsd = find_JSD(&A, &B);
    printf("%s %s %f\n", A.path, B.path, jsd);

    wfd_free(A.wfd);
    wfd_free(B.wfd);
    free(A.path);
    free(B.path);

    return 0;
}