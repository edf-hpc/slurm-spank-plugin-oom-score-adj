/*
 * Copyright (C) 2015 EDF SA
 *
 * Written by Ana Guerrero Lopez <ana-externe.guerrero@edf.fr>
 * Based on spank man page renice example and in slurm-spank-oom-adj
 * by Matthieu Hautreux.
 *
 * This file provides a Slurm SPANK plugin named oom-score-adj.
 * This plugin adjusts the Out-of-Memory (OOM) score of the tasks spawned
 * by Slurm.
 *
 * This software is governed by the CeCILL-C license under French law and
 * abiding by the rules of distribution of free software. You can use,
 * modify and/ or redistribute the software under the terms of the CeCILL-C
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty and the software's author, the holder of the
 * economic rights, and the successive licensors have only limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading, using, modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean that it is complicated to manipulate, and that also
 * therefore means that it is reserved for developers and experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and, more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL-C license and that you accept its terms.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <slurm/spank.h>

/*
 * All spank plugins must define this macro for the Slurm plugin loader.
 */
SPANK_PLUGIN(oom-score-adj, 1);

/*
 * Declare the default value for oom_score_adj to 0.
 * This is also the value by default in the system.
 */
static int value = 0;

static int _str2value (const char *str, int *p2int);
static int _set_oom_score_adj (pid_t pid, int adj);

/*
 *  We don't want to provide an option to srun.
 */
struct spank_option spank_options[] =
{
    SPANK_OPTIONS_TABLE_END
};

/*
 *  Called from both srun and slurmd.
 *  Called just after plugins are loaded. In remote context, this is just
 *  after job step is initialized. This function is called before any
 *  plugin option processing. This function is not called in slurmd
 *  context.
 */
int slurm_spank_init (spank_t sp, int ac, char **av)
{
    int i;

    /* Don't do anything in sbatch/salloc */
    if (spank_context () == S_CTX_ALLOCATOR)
        return (0);

    // Load the default value from the conf file
    for (i = 0; i < ac; i++) {
        if (strncmp ("oom_score_adj=", av[i], 14) == 0) {
            const char *optarg = av[i] + 14;
            if (_str2value (optarg, &value) < 0)
                slurm_error ("oom_score_adj: Ignoring invalid value: '%s'",
                             av[i]);
        } else {
            slurm_error ("oom_score_adj: Invalid value: %s", av[i]);
        }
    }

    if (!spank_remote (sp))
        slurm_verbose ("oom_score_adj: value = %d", value);

    return (0);
}

/*
 * Called for each task just after fork, but before all elevated
 * privileges are dropped. (remote context only)
 */
int slurm_spank_task_init_privileged (spank_t sp, int ac, char **av)
{
    pid_t pid;
    int taskid;

    /* If the value set is the default one, we do nothing */
    if ( value == 0 )
        return (0);

    spank_get_item (sp, S_TASK_GLOBAL_ID, &taskid);
    pid = getpid();

    slurm_info ("set oom_score_adj of task%d (pid %ld) to %ld",
            taskid, pid, value);

    if (_set_oom_score_adj (pid, value) < 0) {
        slurm_error ("oom_score_adj: unable to set oom_score_adj: %m");
        return (-1);
    }

    return (0);
}

/*
 * Converts the string value read from the config file in a integer.
 * If the value is NULL or outside the interval (-1000,1000) returns -1.
 */
static int _str2value (const char *str, int *p2int)
{
    long int l;
    char *p;

    l = strtol (str, &p, 10);
    if ((*p != '\0') || (l < -1000) || (l > 1000))
        return (-1);

    *p2int = (int) l;

    return (0);
}

/*
 * Set /proc/PID/oom_score_adj to the value given.
 */
static int _set_oom_score_adj (pid_t pid, int value)
{
    int fd;
    char oom_score_adj_file[128];
    char oom_score_adj_value[16];

    // Set the value of oom_score_adj_file and open it.
    if (snprintf(oom_score_adj_file, 128, "/proc/%ld/oom_score_adj", pid) >= 128) {
        return -1;
    }

    fd = open(oom_score_adj_file, O_WRONLY);
    if (fd < 0) {
        if (errno == ENOENT)
            debug("%s doesn't exist: %m", oom_score_adj_file);
        else
            error("failed to open %s, error %d: %m", oom_score_adj_file, errno);
        return -1;
    }

    // Set the value of oom_score_adj_value.
    if (snprintf(oom_score_adj_value, 16, "%d", value) >= 16) {
        close(fd);
        return -1;
    }
    // Write the value of oom_score_adj_value in oom_score_adj_file
    // and close the file.
    while ((write(fd, oom_score_adj_value,
                  strlen(oom_score_adj_value)) < 0)
                  &&  (errno == EINTR))
        ;
    close(fd);

    return 0;
}
