/* SPDX-License-Identifier: GPL-3.0-only */
/* OMNI project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h. */
project_t proj;

/* the factory chord buttons, left to right, in C: the primary chords on the first group of three,
 * their relative minors on the next two, then the dominants that lead into them */
static const uint8_t PAD_ROOT[NPADS] = {5, 0, 7, 2, 9, 4, 7, 4, 2, 10, 9};
static const uint8_t PAD_TYPE[NPADS] = {CH_MAJ, CH_MAJ, CH_MAJ, CH_MIN, CH_MIN, CH_MIN, CH_7, CH_7, CH_7, CH_MAJ,
                                        CH_7};

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = om_param_info(i)->def;
    for (i = 0; i < NPADS; i++) {
        proj.pad_root[i] = PAD_ROOT[i];
        proj.pad_type[i] = PAD_TYPE[i];
    }
    proj.leds = 1;
}

static int project_valid(void)
{
    int i;
    if (proj.magic != PROJ_MAGIC || (proj.format != PROJ_FORMAT && proj.format != 1u))
        return 0;
    if (proj.format == 1u) {                     /* 0.1: that slot was Lo-fi (0 / 1) */
        proj.par[P_CREV] = om_param_info(P_CREV)->def;
        proj.format = PROJ_FORMAT;
    }
    for (i = 0; i < P_NPARAMS; i++) {
        const om_param_t *p = om_param_info(i);
        if (proj.par[i] < p->lo || proj.par[i] > p->hi)
            return 0;
    }
    for (i = 0; i < NPADS; i++)
        if (proj.pad_root[i] > 11u || proj.pad_type[i] >= CH_NTYPES)
            return 0;
    return 1;
}

int project_load(void)
{
    int n;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n >= 8 && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void) { return plat_store_save(OBJ_PROJ, &proj, sizeof proj); }

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        om_set(i, proj.par[i]);
}
