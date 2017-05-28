#include <string.h>
#include <stdlib.h>
#include <stdarg.h>


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


vg_cmd *vg_horizto(float pos) {
    vg_cmd *cmd = vg_new_cmd(VG_HORIZ);
    cmd->data = pos;
    return cmd;
}

vg_cmd *vg_vertto(float pos) {
    vg_cmd *cmd = vg_new_cmd(VG_VERT);
    cmd->data = pos;
    return cmd;
}

vg_cmd *vg_moveto(float x, float y) {
    vg_cmd *cmd = vg_new_cmd(VG_MOVE);
    cmd->pt.x = x;
    cmd->pt.y = y;
    return cmd;
}

vg_cmd *vg_lineto(float x, float y) {
    vg_cmd *cmd = vg_new_cmd(VG_LINE);
    cmd->pt.x = x;
    cmd->pt.y = y;
    return cmd;
}

vg_cmd *vg_curveto(float x1, float y1, float x2, float y2, float x3, float y3) {
    vg_cmd *cmd = vg_new_cmd(VG_CURVE);
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
        switch(cmd->type) {
        case VG_VERT:
            drawn_something = 1;
//            fprintf(stderr, "Vert (%f) \n", cmd->data);
            fz_lineto(ctx, fzpath, fz_currentpoint(ctx, fzpath).x, cmd->data);
            break;


        case VG_HORIZ:
            drawn_something = 1;
//            fprintf(stderr, "Horiz (%f) \n", cmd->data);
            fz_lineto(ctx, fzpath, cmd->data, fz_currentpoint(ctx, fzpath).y);
            break;

        case VG_MOVE:
//            fprintf(stderr, "Move (%f,%f) \n", cmd->pt.x, cmd->pt.y);
            fz_moveto(ctx, fzpath, cmd->pt.x, cmd->pt.y);
            break;

        case VG_LINE:
            drawn_something = 1;
//            fprintf(stderr, "Line (%f,%f) \n", cmd->pt.x, cmd->pt.y);
            fz_lineto(ctx, fzpath, cmd->pt.x, cmd->pt.y);
            break;

        case VG_CURVE:
            drawn_something = 1;
//            fprintf(stderr, "Curve (%f,%f) (%f,%f) (%f,%f)\n", cmd->curve.pt1.x, cmd->curve.pt1.y, cmd->curve.pt2.x, cmd->curve.pt2.y, cmd->curve.pt3.x, cmd->curve.pt3.y);
            fz_curveto(ctx, fzpath, cmd->curve.pt1.x, cmd->curve.pt1.y, cmd->curve.pt2.x, cmd->curve.pt2.y, cmd->curve.pt3.x, cmd->curve.pt3.y);
            break;

        case VG_CLOSE:
            if(drawn_something) {
                fz_closepath(ctx, fzpath);
                drawn_something = 0;
            }
            break;
        }
    }

    if(drawn_something && cmd->type != VG_CLOSE) {
//        fprintf(stderr, "Close path(outer)\n");
        fz_closepath(ctx, fzpath);
//    } else {
//        fprintf(stderr, "Not closed path\n");
    }


//    fz_print_path(ctx, fz_stderr(ctx), fzpath, 2);
}

static float logo_bg_color[4] = {(float)0x25/(float)0xFF, (float)0xAC/(float)0xFF, (float)0x72/(float)0xFF, (float)0xFF/(float)0xFF};
static float logo_fg_color[4] = {0.5, 0.5, 0.5, 1};



//vg_fz_pathlist *vg_draw_pathlist(fz_context *ctx, fz_device *dev, fz_rect *rect, fz_matrix *page_ctm, vg_pathlist *pathlist, fz_colorspace *cs, fz_rect *logo_bounds, fz_matrix *logo_tm) {
vg_fz_pathlist *vg_draw_pathlist(fz_context *ctx, fz_device *dev, fz_rect *rect, fz_matrix *page_ctm, vg_pathlist *pathlist) {
    if(pathlist->len == 0) {
        return vg_new_fzpaths(ctx);
    }


    fz_colorspace *cs = fz_device_rgb(ctx);
    vg_fz_pathlist *fzpaths = vg_new_fzpaths(ctx);
    fz_rect annot_rect = *rect;
    fz_path *fzpath;
    fz_rect logo_bounds;
    fz_matrix logo_tm;

    fz_var(logo_bounds);
    fz_var(logo_tm);
    fz_var(annot_rect);
    fz_var(cs);
    fz_var(pathlist);
    fz_var(page_ctm);
    fz_var(fzpath);
    fz_var(fzpaths);
    fz_try(ctx) {
        fz_rect path_bounds;
        fz_rect outer_bounds;

        for(int i = 0; i < pathlist->len; i++) {
            fzpath = fz_new_path(ctx);

            vg_draw_path_cmds(ctx, fzpath, vg_get_path(pathlist, i));
            fz_bound_path(ctx, fzpath, NULL, &fz_identity, &path_bounds);
            fz_union_rect(&outer_bounds, &path_bounds);
            vg_list_append(fzpaths->paths, fzpath);
        }

        center_rect_within_rect(&outer_bounds, rect, &logo_tm);
        fz_concat(&logo_tm, &logo_tm, page_ctm);

        fz_transform_rect(&annot_rect, page_ctm);
        fprintf(stderr, "Matrix (%f, %f, %f, %f, %f, %f)\n", page_ctm->a, page_ctm->b, page_ctm->c, page_ctm->d, page_ctm->e, page_ctm->f);

        for(int i = 0; i < pathlist->len; i++) {
            vg_path *vgpath = vg_get_path(pathlist, i);
            fz_path *fzpath = (fz_path *) vg_list_item(fzpaths->paths, i);

            switch(vgpath->type) {
            case VG_STROKE:
//                fprintf(stderr, "Stroke rgba(%f,%f,%f, %f)\n", vgpath->r, vgpath->g, vgpath->b, vgpath->a);
                fz_begin_group(ctx, dev, &annot_rect, 1, 0, 0, vgpath->a);
                fz_stroke_path(ctx, dev, fzpath, &vgpath->stroke, &logo_tm, cs, vgpath->rgba, vgpath->a);
                fz_end_group(ctx, dev);
                break;

            case VG_FILL:
//                fprintf(stderr, "Fill rgba(%f,%f,%f, %f)\n", vgpath->r, vgpath->g, vgpath->b, vgpath->a);
                fz_begin_group(ctx, dev, &annot_rect, 1, 0, 0, vgpath->a);
                fz_fill_path(ctx, dev, fzpath, 0, &logo_tm, cs, vgpath->rgba, vgpath->a);
                fz_end_group(ctx, dev);
                break;
            }
        }
    } fz_catch(ctx) {
        fz_rethrow(ctx);
    }

    fz_drop_colorspace(ctx, cs);

    return fzpaths;
}
