#include "fill.h"

fill_type fill_tpl_signature_data(pdf_env *env, fill_env *fillenv) {
    json_t *json_sigfile = json_object_get(fillenv->json_map_item, "sigfile");

    if(!json_is_string(json_sigfile) && !env->sigFile) {
        RETURN_FILL_ERROR(fillenv, "Sigfile not set");
    }

    const char *sigfile = env->sigFile ? env->sigFile : json_string_value(json_sigfile);

    struct stat buffer;
    if(stat(sigfile, &buffer) != 0) {
        RETURN_FILL_ERROR(fillenv, "Sigfile not found");
    } else {
        fillenv->sig.file = sigfile;
    }

    json_t *json_pwd = json_object_get(fillenv->json_map_item, "password");

    if(env->sigPwd) {
        fillenv->sig.password = env->sigPwd;
    } else if(json_is_string(json_pwd)) {
        fillenv->sig.password = json_string_value(json_pwd);
    } else {
        RETURN_FILL_ERROR(fillenv, "Password not supplied");
    }

    return ADD_SIGNATURE;
}


fill_type fill_tpl_text_data(pdf_env *env, fill_env *fillenv, fill_type success_type) {
    json_t *json_pos = json_object_get(fillenv->json_map_item, "position");

    if(!json_is_array(json_pos) || json_array_size(json_pos) != 2) {
        RETURN_FILL_ERROR(fillenv, "Position invalid");
    }

    fillenv->text.x = json_number_value(json_array_get(json_pos, 0));
    fillenv->text.y = json_number_value(json_array_get(json_pos, 1));

    json_t *json_width = json_object_get(fillenv->json_map_item, "width");
    if(json_is_number(json_width) && json_number_value(json_width) > 1) {
        fillenv->text.w = json_number_value(json_width);
    } else {
        fillenv->text.w = DEFAULT_WIDTH;
    }

    json_t *json_height = json_object_get(fillenv->json_map_item, "height");
    if(json_is_number(json_height) && json_number_value(json_height) >= 1) {
        fillenv->text.h = json_number_value(json_height);
    } else {
        fillenv->text.h = DEFAULT_HEIGHT;
    }

    json_t *json_edit = json_object_get(fillenv->json_map_item, "editable");
    if(json_is_boolean(json_edit)) {
        fillenv->text.editable = json_boolean_value(json_edit);
    } else {
        fillenv->text.editable = 0;
    }

    json_t *json_font = json_object_get(fillenv->json_map_item, "font");
    if(json_is_string(json_font)) {
        fillenv->text.font = json_string_value(json_font);
    } else {
        fillenv->text.font = 0;
    }

    json_t *json_color= json_object_get(fillenv->json_map_item, "color");
    if(json_is_array(json_color) && json_array_size(json_color) == 3) {
        fillenv->text.color[0] = json_number_value(json_array_get(json_color, 0));
        fillenv->text.color[1] = json_number_value(json_array_get(json_color, 1));
        fillenv->text.color[2] = json_number_value(json_array_get(json_color, 2));
    } else {
        double color = 0;

        if(json_is_number(json_color))
            color = json_number_value(json_color);

        if(color < 0)
            color = 0;
        else if(color > 1)
            color = 1;

        fillenv->text.color[0] = color;
        fillenv->text.color[1] = color;
        fillenv->text.color[2] = color;
    }

    return success_type;
}


fill_type fill_tpl_data(pdf_env *env, fill_env *fillenv) {
    json_t *json_id = json_object_get(fillenv->json_map_item, "id");

    if(json_id != NULL) {
        if (!json_is_integer(json_id)) {
            RETURN_FILL_ERROR(fillenv, "Invalid field id, must be an integer");
        }

        fillenv->field_id = json_integer_value(json_id);
        return FIELD_ID;
    }

    json_t *json_name = json_object_get(fillenv->json_map_item, "name");

    if(json_name != NULL) {
        if (!json_is_string(json_name)) {
            RETURN_FILL_ERROR(fillenv, "Invalid field name, must be a string");
        }

        fillenv->field_name = json_string_value(json_name);
        return FIELD_NAME;
    }

    json_t *json_addtype = json_object_get(fillenv->json_map_item, "add");

    if(json_addtype == NULL) {
        RETURN_FILL_ERROR(fillenv, "Unrecognised template object. Needs an 'id', 'name' or 'add' property.");
    }

    const char *type_name = json_string_value(json_addtype);

    if(strncmp(type_name, "textfield", 13) == 0) {
        return fill_tpl_text_data(env, fillenv, ADD_TEXTFIELD);
    } else if(strncmp(type_name, "signature", 9) == 0) {
        return fill_tpl_signature_data(env, fillenv);
    } else {
        RETURN_FILL_ERROR(fillenv, "Trying to add an unrecognised type. Only 'textfield' and 'signature' supported.");
    }

}
