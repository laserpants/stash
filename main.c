#define _BSD_SOURCE 

#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mbs.h"
#include "window.h"

static volatile bool loop = true;

static void 
sig_handler (int signo) 
{
    if (signo != SIGINT)
        return;

    loop = false;
}

int
main (int argc, char *argv[])
{
    struct mbs state = {
        { 0, 0 },  /* snapshot */
        { 0, 0 },  /* used */
        0,         /* balance */
        0,         /* flags */
        NULL,      /* ifa_name */
        NULL       /* WINDOW */
    };

    struct stats stats = { 0, 0 };

    int status = 0;

    mbs_getopt (argc, argv, &state);

    if (-1 == mbs_poll_interfaces (&state, &stats))
    {
        fprintf (stderr, "No such interface: %s\n", state.ifa_name);
        free (state.ifa_name);
        return EXIT_FAILURE;
    } 

    state.snapshot = stats;

    if (state.flags & FLAG_VERBOSE)
        printf ("Monitoring network interface %s.\n", state.ifa_name);

    setlocale (LC_ALL, "");

    initscr ();

    if (NULL == (state.win = newwin (5, 82, 0, 0)))
    {
        fprintf (stderr, "Error initialising ncurses.\n");
        endwin ();
        free (state.ifa_name);
        return EXIT_FAILURE;
    }

    curs_set (0);
    signal (SIGINT, sig_handler);

    /* Run main loop until SIGINT signal is received. */
    while (true == loop)
    {
        if (-1 == mbs_poll_interfaces (&state, &stats))
        {
            fprintf (stderr, "Interface %s is gone.\n", state.ifa_name);
            status = -1;
            goto exit;
        } 
        else
        {
            const uint64_t tx_diff = stats.tx_bytes - state.snapshot.tx_bytes,
                           rx_diff = stats.rx_bytes - state.snapshot.rx_bytes;

            state.used.tx_bytes += tx_diff;
            state.used.rx_bytes += rx_diff;

            state.snapshot = stats;

            const uint64_t diff = tx_diff + rx_diff;

            if (state.flags & FLAG_COUNTDOWN)
            {
                if (state.balance > diff)
                    state.balance -= diff;
                else
                    state.balance = 0;

                draw_window (&state, !!tx_diff, !!rx_diff);

                if (!state.balance && (state.flags & FLAG_EXIT_ON_0))
                    break;
            }
        }

        usleep (200000);
    }

exit:
    curs_set (1);
    delwin (state.win);
    endwin ();

    if (0 == status && (state.flags & FLAG_COUNTDOWN) && !state.balance)
        printf ("Data limit exceeded.\n");
    else if (0 == status)
        printf ("Terminated!\n");

    free (state.ifa_name);

    return status;
}
