#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>


#include "fill.h"
#include "mupdf/pdf.h"


// path data structures
static void *vg_list_new() {
    vg_list *list = malloc(sizeof(vg_list));

    list->cap = INIT_CAP;
    list->len = 0;
    list->items = malloc(list->cap * sizeof(void*));

    return list;
}

static void *vg_list_append(vg_list *list, void *item) {
    if(list->len == list->cap) {
        int prev_cap = list->cap;
        list->cap = (int) (list->cap * 1.5);
        list->items = realloc(list->items, list->cap * sizeof(void*));
        if(!list->items)
            return 0;
        memset(&list->items[prev_cap], 0, (list->cap - prev_cap) * sizeof(void *));
    }

    list->items[list->len++] = item;

    return item;
}

static void vg_list_free(vg_list *list, void (*free_item)(void *)) {
    if(list == NULL) return;
    for(int i = 0; i < list->len; i++) {
        if(free_item)
            free_item(list->items[i]);
        else
            free(list->items[i]);
    }

    memset(list->items, 0, list->cap * sizeof(void *));
    free(list->items);
    memset(list, 0, sizeof(vg_list));
    free(list);
}

static void *vg_list_item(vg_list *list, int idx) {
    if(idx >= 0 && idx < list->len)
        return list->items[idx];
    else
        return NULL;
}


vg_path *vg_new_path(vg_path_type type, float rgba[4]) {
    vg_path *path = malloc(sizeof(vg_path));

    path->type = type;
    memcpy(path->rgba, rgba, 4 * sizeof(float));

    switch(type) {
    case VG_STROKE:
        memcpy(&path->stroke, &fz_default_stroke_state, sizeof(fz_default_stroke_state));
        break;

    case VG_FILL:
        break;
    }

    path->cmds = vg_list_new();

    return path;
}


static vg_cmd *vg_new_cmd(vg_cmd_type type) {
    vg_cmd *cmd = malloc(sizeof(vg_cmd));
    cmd->type = type;
    return cmd;
}


vg_cmd *vg_horiz(int abs, float pos) {
    vg_cmd *cmd = vg_new_cmd(VG_HORIZ);
    cmd->data = pos;
    cmd->absolute = abs;
    return cmd;
}

vg_cmd *vg_vert(int abs, float pos) {
    vg_cmd *cmd = vg_new_cmd(VG_VERT);
    cmd->data = pos;
    cmd->absolute = abs;
    return cmd;
}

vg_cmd *vg_moveto(int abs, float x, float y) {
    vg_cmd *cmd = vg_new_cmd(VG_MOVE);
    cmd->pt.x = x;
    cmd->pt.y = y;
    cmd->absolute = abs;
    return cmd;
}

vg_cmd *vg_lineto(int abs, float x, float y) {
    vg_cmd *cmd = vg_new_cmd(VG_LINE);
    cmd->pt.x = x;
    cmd->pt.y = y;
    cmd->absolute = abs;
    return cmd;
}

vg_cmd *vg_curveto(int abs, float x1, float y1, float x2, float y2, float x3, float y3) {
    vg_cmd *cmd = vg_new_cmd(VG_CURVE);
    cmd->absolute = abs;
    cmd->curve.pt1.x = x1;
    cmd->curve.pt1.y = y1;
    cmd->curve.pt2.x = x2;
    cmd->curve.pt2.y = y2;
    cmd->curve.pt3.x = x3;
    cmd->curve.pt3.y = y3;
    return cmd;
}

vg_cmd *vg_close() {
    return vg_new_cmd(VG_CLOSE);
}


vg_cmd *vg_get_cmd(vg_path *path, int idx) {
    return (vg_cmd *) vg_list_item(path->cmds, idx);
}

vg_cmd *vg_add_cmd(vg_path *path, vg_cmd *cmd) {
    return (vg_cmd *) vg_list_append(path->cmds, cmd);
}


void vg_free_path(vg_path *path) {
    vg_list_free(path->cmds, NULL);
    memset(path, 0, sizeof(vg_path));
    free(path);
}


vg_pathlist *vg_new_pathlist() {
    return (vg_pathlist *) vg_list_new();
}

vg_path *vg_add_path(vg_pathlist *pathlist, vg_path *path) {
    return (vg_path *) vg_list_append(pathlist, path);
}


vg_path *vg_get_path(vg_pathlist *pathlist, int idx) {
    return (vg_path *) vg_list_item(pathlist, idx);
}


static void vg_free_pathlist_path(void *path) {
    vg_free_path((vg_path*) path);
}

void vg_free_pathlist(vg_pathlist *pathlist) {
    vg_list_free(pathlist, vg_free_pathlist_path);
}



vg_fz_pathlist *vg_new_fzpaths(fz_context *ctx) {
    vg_fz_pathlist *fzpl = malloc(sizeof(vg_fz_pathlist));
    fzpl->ctx = ctx;
    fzpl->paths = vg_list_new();
}



void vg_drop_fz_paths(vg_fz_pathlist *fzpaths) {
    if(fzpaths == NULL) return;

    for(int i=0; i<fzpaths->paths->len; i++) {
        fz_drop_path(fzpaths->ctx, fzpaths->paths->items[i]);
    }

    fzpaths->paths->len = 0;
    vg_list_free(fzpaths->paths, NULL);
    fzpaths->paths->items = NULL;
    free(fzpaths);
}


//a couple of helpers from mupdf pdf-appearance.c

static void rect_center(const fz_rect *rect, fz_point *c) {
    c->x = (rect->x0 + rect->x1) / 2.0f;
    c->y = (rect->y0 + rect->y1) / 2.0f;
}

static void center_rect_within_rect(const fz_rect *tofit, const fz_rect *within, fz_matrix *mat) {
    float xscale = (within->x1 - within->x0) / (tofit->x1 - tofit->x0);
    float yscale = (within->y1 - within->y0) / (tofit->y1 - tofit->y0);
    float scale = fz_min(xscale, yscale);
    fz_point tofit_center;
    fz_point within_center;

    rect_center(within, &within_center);
    rect_center(tofit, &tofit_center);

    /* Translate "tofit" to be centered on the origin
     * Scale "tofit" to a size that fits within "within"
     * Translate "tofit" to "within's" center
     * Do all the above in reverse order so that we can use the fz_pre_xx functions */
    fz_translate(mat, within_center.x, within_center.y);
    fz_pre_scale(mat, scale, -scale);
    fz_pre_translate(mat, -tofit_center.x, -tofit_center.y);
}


// path drawing

static void vg_draw_path_cmds(fz_context *ctx, fz_path *fzpath, vg_path *vgpath) {
    vg_cmd *cmd;
    int drawn_something = 0;
    int c=0;
    for(int i = 0; i < vgpath->cmds->len; i++) {
        cmd = vg_get_cmd(vgpath, i);
        fz_point curr_pt = fz_currentpoint(ctx, fzpath);
        switch(cmd->type) {
        case VG_VERT:
            drawn_something = 1;
//            fprintf(stderr, "Vert (%f) \n", cmd->data);
            if(cmd->absolute)
                fz_lineto(ctx, fzpath, curr_pt.x, cmd->data);
            else
                fz_lineto(ctx, fzpath, curr_pt.x, curr_pt.y + cmd->data);
            break;


        case VG_HORIZ:
            drawn_something = 1;
//            fprintf(stderr, "Horiz (%f) \n", cmd->data);
            if(cmd->absolute)
                fz_lineto(ctx, fzpath, cmd->data, curr_pt.y);
            else
                fz_lineto(ctx, fzpath, curr_pt.x + cmd->data, curr_pt.y);
            break;

        case VG_MOVE:
//            fprintf(stderr, "Move (%f,%f) \n", cmd->pt.x, cmd->pt.y);
            if(cmd->absolute)
                fz_moveto(ctx, fzpath, cmd->pt.x, cmd->pt.y);
            else
                fz_moveto(ctx, fzpath, curr_pt.x + cmd->pt.x, curr_pt.y + cmd->pt.y);
            break;

        case VG_LINE:
            drawn_something = 1;
//            fprintf(stderr, "Line (%f,%f) \n", cmd->pt.x, cmd->pt.y);
            if(cmd->absolute)
                fz_lineto(ctx, fzpath, cmd->pt.x, cmd->pt.y);
            else
                fz_lineto(ctx, fzpath, curr_pt.x + cmd->pt.x, curr_pt.y + cmd->pt.y);
            break;

        case VG_CURVE:
            drawn_something = 1;
//            fprintf(stderr, "Curve (%f,%f) (%f,%f) (%f,%f)\n", cmd->curve.pt1.x, cmd->curve.pt1.y, cmd->curve.pt2.x, cmd->curve.pt2.y, cmd->curve.pt3.x, cmd->curve.pt3.y);
            if(cmd->absolute)
                fz_curveto(ctx, fzpath, cmd->curve.pt1.x, cmd->curve.pt1.y, cmd->curve.pt2.x, cmd->curve.pt2.y, cmd->curve.pt3.x, cmd->curve.pt3.y);
            else
                fz_curveto(ctx, fzpath, curr_pt.x + cmd->curve.pt1.x, curr_pt.y + cmd->curve.pt1.y,
                                        curr_pt.x + cmd->curve.pt2.x, curr_pt.y + cmd->curve.pt2.y,
                                        curr_pt.x + cmd->curve.pt3.x, curr_pt.y + cmd->curve.pt3.y);
            break;

        case VG_CLOSE:
            if(drawn_something) {
//                fprintf(stderr, "close(explicit)\n");
                fz_closepath(ctx, fzpath);
                drawn_something = 0;
            }
            break;
        }
    }

    if(drawn_something && cmd->type != VG_CLOSE) {
//        fprintf(stderr, "close(auto)\n");
        fz_closepath(ctx, fzpath);
    }

//    fz_print_path(ctx, fz_stderr(ctx), fzpath, 2);
}


void vg_draw_pathlist(fz_context *ctx, fz_device *dev, fz_rect *rect, fz_matrix *page_ctm, vg_pathlist *pathlist) {
    if(pathlist->len == 0) {
        return;
    }


    fz_try(ctx) {
        fz_colorspace *cs = fz_device_rgb(ctx);
        fz_rect annot_rect = *rect;
        fz_rect logo_bounds;
        fz_matrix logo_tm;
        fz_rect path_bounds;
        fz_rect outer_bounds;
        fz_path *fzpath;
        fz_path **fzpaths = malloc(pathlist->len * sizeof(void*));

        for(int i = 0; i < pathlist->len; i++) {
            fzpath = fz_new_path(ctx);
            vg_draw_path_cmds(ctx, fzpath, vg_get_path(pathlist, i));
            fz_bound_path(ctx, fzpath, NULL, &fz_identity, &path_bounds);
            fz_union_rect(&outer_bounds, &path_bounds);
            fzpaths[i] = fzpath;
        }

        center_rect_within_rect(&outer_bounds, rect, &logo_tm);
        fz_concat(&logo_tm, &logo_tm, page_ctm);

        fz_transform_rect(&annot_rect, page_ctm);

        for(int i = 0; i < pathlist->len; i++) {
            vg_path *vgpath = vg_get_path(pathlist, i);

            switch(vgpath->type) {
            case VG_STROKE:
//                fprintf(stderr, "Stroke rgba(%f,%f,%f,%f)\n", vgpath->r, vgpath->g, vgpath->b, vgpath->a);
                if(vgpath->a != 1)
                    fz_begin_group(ctx, dev, &annot_rect, 1, 0, 0, vgpath->a);
                fz_stroke_path(ctx, dev, fzpath, &vgpath->stroke, &logo_tm, cs, vgpath->rgba, vgpath->a);
                if(vgpath->a != 1)
                    fz_end_group(ctx, dev);
                break;

            case VG_FILL:
                if(vgpath->a != 1)
                    fz_begin_group(ctx, dev, &annot_rect, 1, 0, 0, vgpath->a);
//                fprintf(stderr, "Fill rgba(%f,%f,%f,%f)\n", vgpath->rgba[0], vgpath->rgba[1], vgpath->rgba[2], vgpath->rgba[3]);
                fz_fill_path(ctx, dev, fzpaths[i], 0, &logo_tm, cs, vgpath->rgba, vgpath->a);
                if(vgpath->a != 1)
                    fz_end_group(ctx, dev);
                break;
            }

            fz_drop_path(ctx, fzpaths[i]);
        }

        free(fzpaths);

        fz_drop_colorspace(ctx, cs);
    } fz_catch(ctx) {
        fz_rethrow(ctx);
    }

    return;
}

// very simple parser for a small subset of svg


typedef struct _vg_parse_state {
    const char *str; //str being parsed
    const char *c;   //curr char in str
//    int tok_offset;
    vg_pathlist *pathlist;  //always valid
    vg_path *path;     //only valid in vg_parse_tok_move/line/etv. not valid in the tok_fill/tok_stroke func
    char token[9];     //need a buffer to collected the tokens 'fill' & 'stroke'
    char *t;           //curr char in tok
    char errmsg[256];
    int found; //keep track of how numbers parsed for a command - used for error messages when less numbers than expected
} vg_parse_state;


//returns how many chars were used trying to read in a number. is zero if failed to read a number.
//has the side effect of incrementing the character offset used by the parser
static int get_one_number(vg_parse_state *state, float *num) {
    char num_str[20];
    const char *start = state->c, *c = state->c;
    while(isspace(*c))
        c++;

    if(*c == '-')
        c++;

    int got_dec_pt = 0;
    while(*c){
        if(*c == '.') {
            if(got_dec_pt) return 0;
            got_dec_pt = 1;
            c++;
            continue;
        }


        if(isdigit(*c)) {
            c++;
            continue;
        }

        //make sure it's not found "" or "."
        if(start == c - got_dec_pt) {
            return 0;
        }

        int len = c - start;

        if (len >= 20) {
            return 0;
        }

        memcpy(num_str, start, len);
        num_str[len] = 0;

       if(!sscanf(num_str, "%e", num))
           return 0; // shouldn't be possible as it's already checked there's a valid number but check anyway

        state->c = c;
        while(isspace(*state->c))
            state->c++;
        state->found++;

        return len;
    };
}


static int get_two_numbers(vg_parse_state *state, float *num1, float *num2) {
    return get_one_number(state, num1) && get_one_number(state, num2);
}

static int get_three_numbers(vg_parse_state *state, float *num1, float *num2, float *num3) {
    return get_one_number(state, num1) && get_one_number(state, num2) && get_one_number(state, num3);
}


static int vg_parse_tok_move(vg_parse_state *state, vg_cmd *cmd) {
    if(get_two_numbers(state, &cmd->pt.x, &cmd->pt.y)) {
        return VG_PARSE_CMD;
    } else {
        sprintf(state->errmsg, "Needed two numbers for move command, found %d", state->found);
        return VG_PARSE_ERROR;
    }
}


static int vg_parse_tok_horiz(vg_parse_state *state, vg_cmd *cmd) {
    if(get_one_number(state, &cmd->data)) {
        return VG_PARSE_CMD;
    } else {
        sprintf(state->errmsg, "Needed two numbers for horiz command, found %d", state->found);
        return VG_PARSE_ERROR;
    }
}

static int vg_parse_tok_vert(vg_parse_state *state, vg_cmd *cmd) {
    if(get_one_number(state, &cmd->data)) {
        return 1;
    } else {
        sprintf(state->errmsg, "Needed two numbers for vert command, found %d", state->found);
        return VG_PARSE_ERROR;
    }
}

static int vg_parse_tok_line(vg_parse_state *state, vg_cmd *cmd) {
    if(get_two_numbers(state, &cmd->pt.x, &cmd->pt.y)) {
        return VG_PARSE_CMD;
    } else {
        sprintf(state->errmsg, "Needed two numbers for line command, found %d", state->found);
        return VG_PARSE_ERROR;
    }
}

static int vg_parse_tok_curve(vg_parse_state *state, vg_cmd *cmd) {
    if(get_two_numbers(state, &cmd->curve.pt1.x, &cmd->curve.pt1.y) &&
       get_two_numbers(state, &cmd->curve.pt2.x, &cmd->curve.pt2.y) &&
       get_two_numbers(state, &cmd->curve.pt3.x, &cmd->curve.pt3.y)) {
        return VG_PARSE_CMD;
    } else {
        sprintf(state->errmsg, "Needed six numbers for curve command, found %d", state->found);
        return VG_PARSE_ERROR;
    }
}

static int vg_parse_tok_close(vg_parse_state *state, vg_cmd *cmd) {
    return VG_PARSE_CMD;
}

static int vg_parse_start_path(vg_parse_state *state, int pathtype) {
    float rgba[4] = {0, 0, 0, 1};

    if(!get_three_numbers(state, &rgba[0], &rgba[1], &rgba[2])) {
        sprintf(state->errmsg, "Need three or four numbers rgba value, found %d", state->found);
        return VG_PARSE_ERROR;
    }

    // if this is unsuccessful then rgba[3] is unchanged and the inital alpha value of 1
    get_one_number(state, rgba+3);

    for(int i = 0; i < 4; i++)
        if(rgba[i] > 1)
            rgba[i] = rgba[i] / 255.f;

    if(!state->path || state->path->cmds->len) {
        state->path = vg_add_path(state->pathlist, vg_new_path(pathtype, rgba));
    } else {
        state->path->type = pathtype;
        memcpy(state->path->rgba, rgba, 4 * sizeof(float));
    }

    return VG_PARSE_PATH;
}

static int vg_parse_tok_fill(vg_parse_state *parse, vg_cmd *cmd) {
    return vg_parse_start_path(parse, VG_FILL);
}

static int vg_parse_tok_stroke(vg_parse_state *parse, vg_cmd *cmd) {
    return vg_parse_start_path(parse, VG_STROKE);
}

typedef struct _vg_parse_func {
    const char *name;
    int (*func)(vg_parse_state *, vg_cmd *cmd);
    int type;
} vg_parse_func;


static vg_parse_func vg_parse_funcs[] = {
    {"m", vg_parse_tok_move, VG_MOVE},
    {"l", vg_parse_tok_line, VG_LINE},
    {"c", vg_parse_tok_curve, VG_CURVE},
    {"h", vg_parse_tok_horiz, VG_HORIZ},
    {"v", vg_parse_tok_vert, VG_VERT},
    {"z", vg_parse_tok_close, VG_CLOSE},
    {"fill", vg_parse_tok_fill, VG_FILL},
    {"stroke", vg_parse_tok_stroke, VG_STROKE},
    {NULL, NULL}
};

vg_parse_func *vg_parse_match_tok(vg_parse_state *state) {
    vg_parse_func *pfunc = vg_parse_funcs;
    while(pfunc->name != NULL) {
        if(strncmp(pfunc->name, state->token, strlen(pfunc->name)) == 0) {
            state->t = state->token;
            *state->t = 0;
            return pfunc;
        }
        pfunc++;
    }

    return NULL;
}

vg_pathlist *vg_parse_str(const char *msg) {
    vg_parse_state state = {0};

    state.str = msg;
    state.pathlist = vg_new_pathlist();
    state.t = state.token;

    state.c = state.str;

    if(!*state.c) return NULL;

//    fprintf(stderr, "PARSE:%s\n", msg);

    while(isspace(*state.c))
        state.c++;

    do {
        char ch = *state.c++;
        *state.t++ = tolower(ch);
        *state.t = 0;

        if(state.t - state.token > 8)    {
            fprintf(stderr, "Invalid path : Token '%s' not recognised at pos %d in gfx str at:\n%.32s\n", state.token, (int) (state.c - state.str), state.c);
            exit(-1);
        }

        vg_parse_func *matched= vg_parse_match_tok(&state);

        if(matched == NULL) {
            continue;
        }

        state.found = 0;
        const char *start = state.c - strlen(matched->name);
        vg_cmd *cmd = vg_new_cmd(matched->type);
        switch(matched->func(&state, cmd)) {
        case VG_PARSE_ERROR:
            fprintf(stderr, "Error: %s.\nAt pos %d at %s \n", state.errmsg, (int) (start - state.str), start);
            free(cmd);
            exit(-1);

        case VG_PARSE_CMD:
            vg_add_cmd(state.path, cmd);
            cmd->absolute = isupper(ch) > 0 ? 1 : 0;
            break;

        case VG_PARSE_PATH:
            free(cmd);
            break;

        }
    } while(*state.c);

    if(!state.pathlist->len) {
        free(state.pathlist);
        state.pathlist = NULL;
    }

    return state.pathlist;
}
