#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#if !defined(FCODER_DEFAULT_BINDINGS_CPP)
#define FCODER_DEFAULT_BINDINGS_CPP

#include "4coder_default_include.cpp"

#include "lib/external/4coder-clearfeld/packages/relative_line_number_mode/relative_line_number_mode.cpp"

#include "lib/external/4coder-vimmish/4coder_vimmish.cpp"

#if !defined(META_PASS)
#include "generated/managed_id_metadata.cpp"
#endif

function void
custom_render_buffer(Application_Links *app, View_ID view_id,
                     Face_ID face_id, Buffer_ID buffer,
                     Text_Layout_ID text_layout_id,
                     Rect_f32 rect) {
    ProfileScope(app, "render buffer");
    
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);
    
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    
    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    f32 cursor_roundness =
        metrics.normal_advance * global_config.cursor_roundness;
    f32 mark_thickness = (f32)global_config.mark_thickness;
    
    // NOTE(allen): Token colorizing
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    if (token_array.tokens != 0) {
        draw_cpp_token_colors(app, text_layout_id, &token_array);
        
        // NOTE(allen): Scan for TODOs and NOTEs
        if (global_config.use_comment_keyword) {
            Comment_Highlight_Pair pairs[] = {
                {string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0)},
                {string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1)},
            };
            draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs,
                                    ArrayCount(pairs));
        }
    } else {
        paint_text_color_fcolor(app, text_layout_id, visible_range,
                                fcolor_id(defcolor_text_default));
    }
    
    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);
    
    // NOTE(allen): Scope highlight
    if (global_config.use_scope_highlight) {
        Color_Array colors = finalize_color_array(defcolor_back_cycle);
        draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals,
                             colors.count);
    }
    
    if (global_config.use_error_highlight || global_config.use_jump_highlight) {
        // NOTE(allen): Error highlight
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        if (global_config.use_error_highlight) {
            draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer,
                                 fcolor_id(defcolor_highlight_junk));
        }
        
        // NOTE(allen): Search highlight
        if (global_config.use_jump_highlight) {
            Buffer_ID jump_buffer = get_locked_jump_buffer(app);
            if (jump_buffer != compilation_buffer) {
                draw_jump_highlights(app, buffer, text_layout_id, jump_buffer,
                                     fcolor_id(defcolor_highlight_white));
            }
        }
    }
    
    // NOTE(allen): Color parens
    if (global_config.use_paren_helper) {
        Color_Array colors = finalize_color_array(defcolor_text_cycle);
        draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals,
                             colors.count);
    }
    
    // NOTE(allen): Line highlight
    if (global_config.highlight_line_at_cursor && is_active_view) {
        i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
        draw_line_highlight(app, text_layout_id, line_number,
                            fcolor_id(defcolor_highlight_cursor_line));
    }
    
    // NOTE(allen): Whitespace highlight
    b64 show_whitespace = false;
    view_get_setting(app, view_id, ViewSetting_ShowWhitespace, &show_whitespace);
    if (show_whitespace) {
        if (token_array.tokens == 0) {
            draw_whitespace_highlight(app, buffer, text_layout_id, cursor_roundness);
        } else {
            draw_whitespace_highlight(app, text_layout_id, &token_array,
                                      cursor_roundness);
        }
    }
    
    // NOTE(allen): Cursor
    switch (fcoder_mode) {
        case FCoderMode_Original: {
            draw_original_4coder_style_cursor_mark_highlight(
                                                             app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness,
                                                             mark_thickness);
        } break;
        case FCoderMode_NotepadLike: {
            draw_notepad_style_cursor_highlight(app, view_id, buffer, text_layout_id,
                                                cursor_roundness);
        } break;
    }
    
    // NOTE(allen): Fade ranges
    paint_fade_ranges(app, text_layout_id, buffer);
    
    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);
    
    draw_set_clip(app, prev_clip);
}


function void
custom_render_caller(Application_Links *app, Frame_Info frame_info, View_ID view_id) {
    ProfileScope(app, "default render caller");
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    
    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;
    f32 digit_advance = face_metrics.decimal_digit_advance;
    
    Rect_f32 region = vim_draw_background_and_margin(app, view_id, is_active_view, line_height + 2.0f);
    Rect_f32 prev_clip = draw_set_clip(app, region);
    
    // NOTE(allen): file bar
    b64 showing_file_bar = false;
    if (view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar){
#if VIM_FILE_BAR_ON_BOTTOM
        Rect_f32_Pair pair = layout_file_bar_on_bot(region, line_height);
        vim_draw_file_bar(app, view_id, buffer, face_id, pair.max);
        region = pair.min;
#else
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        vim_draw_file_bar(app, view_id, buffer, face_id, pair.min);
        region = pair.max;
#endif
    }
    
    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);
    
    Buffer_Point_Delta_Result delta = delta_apply(app, view_id,
                                                  frame_info.animation_dt, scroll);
    if (!block_match_struct(&scroll.position, &delta.point)){
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }
    
    // NOTE(allen): query bars
    {
        Query_Bar *space[32];
        Query_Bar_Ptr_Array query_bars = {};
        query_bars.ptrs = space;
        if (get_active_query_bars(app, view_id, ArrayCount(space), &query_bars)){
            for (i32 i = 0; i < query_bars.count; i += 1){
                Rect_f32_Pair pair = layout_query_bar_on_top(region, line_height, 1);
                draw_query_bar(app, query_bars.ptrs[i], face_id, pair.min);
                region = pair.max;
            }
        }
    }
    
    // NOTE(allen): FPS hud
    if (show_fps_hud){
        Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
        draw_fps_hud(app, frame_info, face_id, pair.max);
        region = pair.min;
        animate_in_n_milliseconds(app, 1000);
    }
    
    // NOTE(allen): layout line numbers
    Rect_f32 line_number_rect = {};
    if (global_config.show_line_number_margins){
        Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        region = pair.max;
    }
    
    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
    
    // NOTE(allen): draw line numbers
    use_relative_line_number_mode = true;
    if(global_config.show_line_number_margins) {
        if(use_relative_line_number_mode) {
            draw_relative_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
        } else {
            draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
        }
    }
    // NOTE(allen): draw the buffer
    vim_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);
    
    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}


function void
custom_setup_default_mapping(Application_Links* app, Mapping *mapping, Vim_Key vim_leader) {
    MappingScope();
    SelectMapping(mapping);
    
    //
    // Global Map
    //
    
    SelectMap(mapid_global);
    
    BindCore(default_startup,          CoreCode_Startup);
    BindCore(default_try_exit,         CoreCode_TryExit);
    Bind(project_go_to_root_directory, KeyCode_H,      KeyCode_Control);
    Bind(toggle_fullscreen,            KeyCode_Alt,    KeyCode_Return);
    Bind(save_all_dirty_buffers,       KeyCode_S,      KeyCode_Control, KeyCode_Shift);
    Bind(change_to_build_panel,        KeyCode_Period, KeyCode_Alt);
    Bind(close_build_panel,            KeyCode_Comma,  KeyCode_Alt);
    Bind(goto_next_jump,               KeyCode_N,      KeyCode_Control);
    Bind(goto_prev_jump,               KeyCode_P,      KeyCode_Control);
    Bind(build_in_build_panel,         KeyCode_M,      KeyCode_Alt);
    
    Bind(goto_first_jump,                   KeyCode_M, KeyCode_Alt, KeyCode_Shift);
    Bind(toggle_filebar,                    KeyCode_B, KeyCode_Alt);
    Bind(execute_any_cli,                   KeyCode_Z, KeyCode_Alt);
    Bind(execute_previous_cli,              KeyCode_Z, KeyCode_Alt, KeyCode_Shift);
    Bind(command_lister,                    KeyCode_X, KeyCode_Alt);
    Bind(project_command_lister,            KeyCode_X, KeyCode_Alt, KeyCode_Shift);
    Bind(list_all_functions_current_buffer, KeyCode_I, KeyCode_Control, KeyCode_Shift);
    Bind(project_fkey_command,              KeyCode_F1);
    Bind(project_fkey_command,              KeyCode_F2);
    Bind(project_fkey_command,              KeyCode_F3);
    Bind(project_fkey_command,              KeyCode_F4);
    Bind(project_fkey_command,              KeyCode_F5);
    Bind(project_fkey_command,              KeyCode_F6);
    Bind(project_fkey_command,              KeyCode_F7);
    Bind(project_fkey_command,              KeyCode_F8);
    Bind(project_fkey_command,              KeyCode_F9);
    Bind(project_fkey_command,              KeyCode_F12);
    Bind(project_fkey_command,              KeyCode_F13);
    Bind(project_fkey_command,              KeyCode_F14);
    Bind(project_fkey_command,              KeyCode_F15);
    Bind(project_fkey_command,              KeyCode_F16);
    Bind(exit_4coder,                       KeyCode_F4, KeyCode_Alt);
    BindMouseWheel(mouse_wheel_scroll);
    BindMouseWheel(mouse_wheel_change_face_size, KeyCode_Control);
    
    //
    // File Map
    //
    
    SelectMap(mapid_file);
    ParentMap(mapid_global);
    
    BindMouse(click_set_cursor_and_mark, MouseCode_Left);
    BindMouseRelease(click_set_cursor, MouseCode_Left);
    BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
    BindMouseMove(click_set_cursor_if_lbutton);
    
    BindTextInput(write_text_input);
    
    Bind(delete_char,                                 KeyCode_Delete);
    Bind(backspace_char,                              KeyCode_Backspace);
    Bind(move_up,                                     KeyCode_Up);
    Bind(move_down,                                   KeyCode_Down);
    Bind(move_left,                                   KeyCode_Left);
    Bind(move_right,                                  KeyCode_Right);
    Bind(seek_end_of_line,                            KeyCode_End);
    Bind(seek_beginning_of_line,                      KeyCode_Home);
    Bind(page_up,                                     KeyCode_PageUp);
    Bind(page_down,                                   KeyCode_PageDown);
    Bind(goto_beginning_of_file,                      KeyCode_PageUp, KeyCode_Control);
    Bind(goto_end_of_file,                            KeyCode_PageDown, KeyCode_Control);
    Bind(move_up_to_blank_line_end,                   KeyCode_Up, KeyCode_Control);
    Bind(move_down_to_blank_line_end,                 KeyCode_Down, KeyCode_Control);
    Bind(move_left_whitespace_boundary,               KeyCode_Left, KeyCode_Control);
    Bind(move_right_whitespace_boundary,              KeyCode_Right, KeyCode_Control);
    Bind(move_line_up,                                KeyCode_Up, KeyCode_Alt);
    Bind(move_line_down,                              KeyCode_Down, KeyCode_Alt);
    Bind(backspace_alpha_numeric_boundary,            KeyCode_Backspace, KeyCode_Control);
    Bind(delete_alpha_numeric_boundary,               KeyCode_Delete, KeyCode_Control);
    Bind(snipe_backward_whitespace_or_token_boundary, KeyCode_Backspace, KeyCode_Alt);
    Bind(snipe_forward_whitespace_or_token_boundary,  KeyCode_Delete, KeyCode_Alt);
    Bind(set_mark,                                    KeyCode_Space, KeyCode_Control);
    Bind(copy,                                        KeyCode_C, KeyCode_Control);
    Bind(delete_range,                                KeyCode_D, KeyCode_Control);
    Bind(delete_line,                                 KeyCode_D, KeyCode_Control, KeyCode_Shift);
    Bind(goto_line,                                   KeyCode_G, KeyCode_Control);
    Bind(paste_and_indent,                            KeyCode_V, KeyCode_Control);
    Bind(paste_next_and_indent,                       KeyCode_V, KeyCode_Control, KeyCode_Shift);
    Bind(cut,                                         KeyCode_X, KeyCode_Control);
    Bind(vim_redo,                                    KeyCode_Y, KeyCode_Control);
    Bind(vim_undo,                                    KeyCode_Z, KeyCode_Control);
    Bind(if_read_only_goto_position,                  KeyCode_Return);
    Bind(if_read_only_goto_position_same_panel,       KeyCode_Return, KeyCode_Shift);
    Bind(view_jump_list_with_lister,                  KeyCode_Period, KeyCode_Control, KeyCode_Shift);
    Bind(vim_enter_normal_mode_escape,                KeyCode_Escape);
    
    //
    // Code Map
    //
    
    SelectMap(mapid_code);
    ParentMap(mapid_file);
    
    BindMouse(click_set_cursor_and_mark, MouseCode_Left);
    BindMouseRelease(click_set_cursor, MouseCode_Left);
    BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
    BindMouseMove(click_set_cursor_if_lbutton);
    
    BindTextInput(vim_write_text_abbrev_and_auto_indent);
    
    Bind(move_left_alpha_numeric_boundary,                    KeyCode_Left, KeyCode_Control);
    Bind(move_right_alpha_numeric_boundary,                   KeyCode_Right, KeyCode_Control);
    Bind(move_left_alpha_numeric_or_camel_boundary,           KeyCode_Left, KeyCode_Alt);
    Bind(move_right_alpha_numeric_or_camel_boundary,          KeyCode_Right, KeyCode_Alt);
    Bind(vim_backspace_char,                                  KeyCode_Backspace);
    Bind(comment_line_toggle,                                 KeyCode_Semicolon, KeyCode_Control);
    Bind(word_complete,                                       KeyCode_Tab);
    // Bind(word_complete_drop_down,                             KeyCode_N, KeyCode_Control);
    Bind(auto_indent_range,                                   KeyCode_Tab, KeyCode_Control);
    Bind(auto_indent_line_at_cursor,                          KeyCode_Tab, KeyCode_Shift);
    Bind(word_complete_drop_down,                             KeyCode_Tab, KeyCode_Shift, KeyCode_Control);
    Bind(write_block,                                         KeyCode_R, KeyCode_Alt);
    Bind(write_todo,                                          KeyCode_T, KeyCode_Alt);
    Bind(write_note,                                          KeyCode_Y, KeyCode_Alt);
    Bind(list_all_locations_of_type_definition,               KeyCode_D, KeyCode_Alt);
    Bind(list_all_locations_of_type_definition_of_identifier, KeyCode_T, KeyCode_Alt, KeyCode_Shift);
    Bind(open_long_braces,                                    KeyCode_LeftBracket, KeyCode_Control);
    Bind(open_long_braces_semicolon,                          KeyCode_LeftBracket, KeyCode_Control, KeyCode_Shift);
    Bind(open_long_braces_break,                              KeyCode_RightBracket, KeyCode_Control, KeyCode_Shift);
    Bind(select_surrounding_scope,                            KeyCode_LeftBracket, KeyCode_Alt);
    Bind(select_surrounding_scope_maximal,                    KeyCode_LeftBracket, KeyCode_Alt, KeyCode_Shift);
    Bind(select_prev_scope_absolute,                          KeyCode_RightBracket, KeyCode_Alt);
    Bind(select_prev_top_most_scope,                          KeyCode_RightBracket, KeyCode_Alt, KeyCode_Shift);
    Bind(select_next_scope_absolute,                          KeyCode_Quote, KeyCode_Alt);
    Bind(select_next_scope_after_current,                     KeyCode_Quote, KeyCode_Alt, KeyCode_Shift);
    Bind(place_in_scope,                                      KeyCode_ForwardSlash, KeyCode_Alt);
    Bind(delete_current_scope,                                KeyCode_Minus, KeyCode_Alt);
    Bind(if0_off,                                             KeyCode_I, KeyCode_Alt);
    Bind(open_file_in_quotes,                                 KeyCode_1, KeyCode_Alt);
    Bind(open_matching_file_cpp,                              KeyCode_2, KeyCode_Alt);
    Bind(write_zero_struct,                                   KeyCode_0, KeyCode_Control);
    
    //
    // Normal Map
    //
    
    SelectMap(vim_mapid_normal);
    ParentMap(mapid_global);
    
    BindMouse(vim_start_mouse_select, MouseCode_Left);
    BindMouseRelease(click_set_cursor, MouseCode_Left);
    BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
    BindMouseMove(vim_mouse_drag);
    
    //
    // Visual Map
    //
    
    SelectMap(vim_mapid_visual);
    ParentMap(vim_mapid_normal);
    
    //
    // ...
    //
    
    //
    // Vim Maps
    //
    
    VimMappingScope();
    
    //
    // Text Object Vim Map
    //
    
    VimSelectMap(vim_map_text_objects);
    
    VimBind(vim_text_object_inner_scope,                         vim_key(KeyCode_I), vim_key(KeyCode_LeftBracket, KeyCode_Shift));
    VimBind(vim_text_object_inner_scope,                         vim_key(KeyCode_I), vim_key(KeyCode_RightBracket, KeyCode_Shift));
    VimBind(vim_text_object_inner_paren,                         vim_key(KeyCode_I), vim_key(KeyCode_9, KeyCode_Shift));
    VimBind(vim_text_object_inner_paren,                         vim_key(KeyCode_I), vim_key(KeyCode_0, KeyCode_Shift));
    VimBind(vim_text_object_inner_single_quotes,                 vim_key(KeyCode_I), vim_key(KeyCode_Quote));
    VimBind(vim_text_object_inner_double_quotes,                 vim_key(KeyCode_I), vim_key(KeyCode_Quote, KeyCode_Shift));
    VimBind(vim_text_object_inner_word,                          vim_key(KeyCode_I), vim_key(KeyCode_W));
    VimBind(vim_text_object_isearch_repeat_forward,              vim_key(KeyCode_G), vim_key(KeyCode_N));
    VimBind(vim_text_object_isearch_repeat_backward,             vim_key(KeyCode_G), vim_key(KeyCode_N, KeyCode_Shift));
    
    //
    // Operator Pending Vim Map
    //
    
    VimSelectMap(vim_map_operator_pending);
    VimAddParentMap(vim_map_text_objects);
    
    VimBind(vim_motion_left,                                        vim_key(KeyCode_H));
    VimBind(vim_motion_down,                                        vim_key(KeyCode_J));
    VimBind(vim_motion_up,                                          vim_key(KeyCode_K));
    VimBind(vim_motion_right,                                       vim_key(KeyCode_L));
    VimBind(vim_motion_left,                                        vim_key(KeyCode_Left));
    VimBind(vim_motion_down,                                        vim_key(KeyCode_Down));
    VimBind(vim_motion_up,                                          vim_key(KeyCode_Up));
    VimBind(vim_motion_right,                                       vim_key(KeyCode_Right));
    VimBind(vim_motion_to_empty_line_down,                          vim_key(KeyCode_RightBracket, KeyCode_Shift));
    VimBind(vim_motion_to_empty_line_up,                            vim_key(KeyCode_LeftBracket,  KeyCode_Shift));
    VimBind(vim_motion_word,                                        vim_key(KeyCode_W));
    VimBind(vim_motion_big_word,                                    vim_key(KeyCode_W, KeyCode_Shift));
    VimBind(vim_motion_word_end,                                    vim_key(KeyCode_E));
    VimBind(vim_motion_word_backward,                               vim_key(KeyCode_B));
    VimBind(vim_motion_big_word_backward,                           vim_key(KeyCode_B, KeyCode_Shift));
    VimBind(vim_motion_line_start_textual,                          vim_key(KeyCode_0));
    VimBind(vim_motion_line_start_textual,                          vim_key(KeyCode_6, KeyCode_Shift));
    VimBind(vim_motion_line_end_textual,                            vim_key(KeyCode_4, KeyCode_Shift));
    VimBind(vim_motion_scope,                                       vim_key(KeyCode_5, KeyCode_Shift));
    VimBind(vim_motion_buffer_start_or_goto_line,                   vim_key(KeyCode_G),           vim_key(KeyCode_G));
    VimBind(vim_motion_buffer_end_or_goto_line,                     vim_key(KeyCode_G, KeyCode_Shift));
    VimBind(vim_motion_page_top,                                    vim_key(KeyCode_H, KeyCode_Shift));
    VimBind(vim_motion_page_mid,                                    vim_key(KeyCode_M, KeyCode_Shift));
    VimBind(vim_motion_page_bottom,                                 vim_key(KeyCode_L, KeyCode_Shift));
    VimBind(vim_motion_find_character_case_sensitive,               vim_key(KeyCode_F));
    VimBind(vim_motion_find_character_backward_case_sensitive,      vim_key(KeyCode_F, KeyCode_Shift));
    VimBind(vim_motion_to_character_case_sensitive,                 vim_key(KeyCode_T));
    VimBind(vim_motion_to_character_backward_case_sensitive,        vim_key(KeyCode_T, KeyCode_Shift));
    VimBind(vim_motion_find_character_pair_case_sensitive,          vim_key(KeyCode_S));
    VimBind(vim_motion_find_character_pair_backward_case_sensitive, vim_key(KeyCode_S, KeyCode_Shift));
    VimBind(vim_motion_repeat_character_seek_same_direction,        vim_key(KeyCode_Semicolon));
    VimBind(vim_motion_repeat_character_seek_reverse_direction,     vim_key(KeyCode_Comma));
    
    //
    // Normal Vim Map
    //
    
    VimSelectMap(vim_map_normal);
    VimAddParentMap(vim_map_operator_pending);
    
    VimBind(comment_line, vim_key(KeyCode_Control, KeyCode_ForwardSlash));
    VimBind(vim_register,                                        vim_key(KeyCode_Quote, KeyCode_Shift));
    VimBind(vim_change,                                          vim_key(KeyCode_C));
    VimBind(vim_change_eol,                                      vim_key(KeyCode_C, KeyCode_Shift));
    VimBind(vim_delete,                                          vim_key(KeyCode_D));
    VimBind(vim_delete_eol,                                      vim_key(KeyCode_D, KeyCode_Shift));
    VimBind(vim_delete_character,                                vim_key(KeyCode_X));
    VimBind(vim_yank,                                            vim_key(KeyCode_Y));
    VimBind(vim_yank_eol,                                        vim_key(KeyCode_Y, KeyCode_Shift));
    VimBind(vim_paste,                                           vim_key(KeyCode_P));
    VimBind(vim_paste_pre_cursor,                                vim_key(KeyCode_P, KeyCode_Shift));
    VimBind(vim_auto_indent,                                     vim_key(KeyCode_Equal));
    VimBind(vim_indent,                                          vim_key(KeyCode_Period, KeyCode_Shift));
    VimBind(vim_outdent,                                         vim_key(KeyCode_Comma, KeyCode_Shift));
    VimBind(vim_replace,                                         vim_key(KeyCode_R));
    VimBind(vim_new_line_below,                                  vim_key(KeyCode_O))->flags |= VimBindingFlag_TextCommand;
    VimBind(vim_new_line_above,                                  vim_key(KeyCode_O, KeyCode_Shift))->flags |= VimBindingFlag_TextCommand;
    VimBind(vim_join_line,                                       vim_key(KeyCode_J, KeyCode_Shift));
    VimBind(vim_align,                                           vim_key(KeyCode_G), vim_key(KeyCode_L));
    VimBind(vim_align_right,                                     vim_key(KeyCode_G), vim_key(KeyCode_L, KeyCode_Shift));
    VimBind(vim_align_string,                                    vim_key(KeyCode_G), vim_key(KeyCode_L, KeyCode_Control));
    VimBind(vim_align_string_right,                              vim_key(KeyCode_G), vim_key(KeyCode_L, KeyCode_Shift, KeyCode_Control));
    VimBind(vim_lowercase,                                       vim_key(KeyCode_G), vim_key(KeyCode_U));
    VimBind(vim_uppercase,                                       vim_key(KeyCode_G), vim_key(KeyCode_U, KeyCode_Shift));
    VimBind(vim_miblo_increment,                                 vim_key(KeyCode_A, KeyCode_Control));
    VimBind(vim_miblo_decrement,                                 vim_key(KeyCode_X, KeyCode_Control));
    VimBind(vim_miblo_increment_sequence,                        vim_key(KeyCode_G), vim_key(KeyCode_A, KeyCode_Control));
    VimBind(vim_miblo_decrement_sequence,                        vim_key(KeyCode_G), vim_key(KeyCode_X, KeyCode_Control));
    
    // @TODO: Doing that ->flags thing is a bit weird...
    VimBind(vim_enter_insert_mode,                               vim_key(KeyCode_I))->flags |= VimBindingFlag_TextCommand;
    VimBind(vim_enter_insert_sol_mode,                           vim_key(KeyCode_I, KeyCode_Shift))->flags |= VimBindingFlag_TextCommand;
    VimBind(vim_enter_append_mode,                               vim_key(KeyCode_A))->flags |= VimBindingFlag_TextCommand;
    VimBind(vim_enter_append_eol_mode,                           vim_key(KeyCode_A, KeyCode_Shift))->flags |= VimBindingFlag_TextCommand;
    VimBind(vim_toggle_visual_mode,                              vim_key(KeyCode_V));
    VimBind(vim_toggle_visual_line_mode,                         vim_key(KeyCode_V, KeyCode_Shift));
    VimBind(vim_toggle_visual_block_mode,                        vim_key(KeyCode_V, KeyCode_Control));
    
    VimBind(change_active_panel,                                 vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_W));
    VimBind(change_active_panel,                                 vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_W, KeyCode_Control));
    VimBind(swap_panels,                                         vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_X));
    VimBind(swap_panels,                                         vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_X, KeyCode_Control));
    VimBind(windmove_panel_left,                                 vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_H));
    VimBind(windmove_panel_left,                                 vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_H, KeyCode_Control));
    VimBind(windmove_panel_down,                                 vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_J));
    VimBind(windmove_panel_down,                                 vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_J, KeyCode_Control));
    VimBind(windmove_panel_up,                                   vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_K));
    VimBind(windmove_panel_up,                                   vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_K, KeyCode_Control));
    VimBind(windmove_panel_right,                                vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_L));
    VimBind(windmove_panel_right,                                vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_L, KeyCode_Control));
    VimBind(windmove_panel_swap_left,                            vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_H, KeyCode_Shift));
    VimBind(windmove_panel_swap_left,                            vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_H, KeyCode_Shift, KeyCode_Control));
    VimBind(windmove_panel_swap_down,                            vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_J, KeyCode_Shift));
    VimBind(windmove_panel_swap_down,                            vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_J, KeyCode_Shift, KeyCode_Control));
    VimBind(windmove_panel_swap_up,                              vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_K, KeyCode_Shift));
    VimBind(windmove_panel_swap_up,                              vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_K, KeyCode_Shift, KeyCode_Control));
    VimBind(windmove_panel_swap_right,                           vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_L, KeyCode_Shift));
    VimBind(windmove_panel_swap_right,                           vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_L, KeyCode_Shift, KeyCode_Control));
    VimBind(vim_split_window_vertical,                           vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_V));
    VimBind(vim_split_window_vertical,                           vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_V, KeyCode_Control));
    VimBind(vim_split_window_horizontal,                         vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_S));
    VimBind(vim_split_window_horizontal,                         vim_key(KeyCode_W, KeyCode_Control), vim_key(KeyCode_S, KeyCode_Control));
    
    VimBind(center_view,                                         vim_key(KeyCode_Z), vim_key(KeyCode_Z));
    VimBind(vim_view_move_line_to_top,                           vim_key(KeyCode_Z), vim_key(KeyCode_T));
    VimBind(vim_view_move_line_to_bottom,                        vim_key(KeyCode_Z), vim_key(KeyCode_B));
    
    VimBind(vim_page_up,                                         vim_key(KeyCode_B, KeyCode_Control));
    VimBind(vim_page_down,                                       vim_key(KeyCode_F, KeyCode_Control));
    VimBind(vim_half_page_up,                                    vim_key(KeyCode_U, KeyCode_Control));
    VimBind(vim_half_page_down,                                  vim_key(KeyCode_D, KeyCode_Control));
    
    VimBind(vim_step_back_jump_history,                          vim_key(KeyCode_O, KeyCode_Control));
    VimBind(vim_step_forward_jump_history,                       vim_key(KeyCode_I, KeyCode_Control));
    VimBind(vim_previous_buffer,                                 vim_key(KeyCode_6, KeyCode_Control));
    
    VimBind(vim_record_macro,                                    vim_key(KeyCode_Q));
    VimBind(vim_replay_macro,                                    vim_key(KeyCode_2, KeyCode_Shift))->flags |= VimBindingFlag_TextCommand;
    VimBind(vim_set_mark,                                        vim_key(KeyCode_M));
    VimBind(vim_go_to_mark,                                      vim_key(KeyCode_Quote));
    VimBind(vim_go_to_mark,                                      vim_key(KeyCode_Tick));
    VimBind(vim_go_to_mark_less_history,                         vim_key(KeyCode_G), vim_key(KeyCode_Quote));
    VimBind(vim_go_to_mark_less_history,                         vim_key(KeyCode_G), vim_key(KeyCode_Tick));
    VimBind(vim_open_file_in_quotes_in_same_window,              vim_key(KeyCode_G), vim_key(KeyCode_F));
    VimBind(vim_jump_to_definition_under_cursor,                 vim_key(KeyCode_RightBracket, KeyCode_Control));
    
    VimNameBind(string_u8_litexpr("Project"),                    vim_leader, vim_key(KeyCode_P));
    VimBind(load_project,                                        vim_leader, vim_key(KeyCode_P), vim_key(KeyCode_O));
    
    VimNameBind(string_u8_litexpr("Buffer"),                    vim_leader, vim_key(KeyCode_B));
    VimBind(interactive_switch_buffer,                           vim_leader, vim_key(KeyCode_B), vim_key(KeyCode_B));
    VimBind(interactive_kill_buffer,                             vim_leader, vim_key(KeyCode_B), vim_key(KeyCode_K));
    VimBind(kill_buffer,                                         vim_leader, vim_key(KeyCode_B), vim_key(KeyCode_D));
    
    VimNameBind(string_u8_litexpr("Files"),                      vim_leader, vim_key(KeyCode_F));
    VimBind(interactive_new,                                     vim_leader, vim_key(KeyCode_F), vim_key(KeyCode_N));
    VimBind(interactive_open_or_new,                             vim_leader, vim_key(KeyCode_F), vim_key(KeyCode_O));
    VimBind(q,                                                   vim_leader, vim_key(KeyCode_F), vim_key(KeyCode_Q));
    VimBind(qa,                                                  vim_leader, vim_key(KeyCode_F), vim_key(KeyCode_Q, KeyCode_Shift));
    VimBind(qa,                                                  vim_leader, vim_key(KeyCode_F), vim_key(KeyCode_Q, KeyCode_Shift));
    VimBind(w,                                                   vim_leader, vim_key(KeyCode_F), vim_key(KeyCode_W));
    
    VimNameBind(string_u8_litexpr("Search"),                     vim_leader, vim_key(KeyCode_S));
    VimBind(list_all_substring_locations_case_insensitive,       vim_leader, vim_key(KeyCode_S), vim_key(KeyCode_S));
    
    VimNameBind(string_u8_litexpr("Tags"),                       vim_leader, vim_key(KeyCode_T));
    VimBind(jump_to_definition,                                  vim_leader, vim_key(KeyCode_T), vim_key(KeyCode_A));
    
    
    VimNameBind(string_u8_litexpr("Window"),                       vim_leader, vim_key(KeyCode_W));
    VimBind(windmove_panel_down, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_J));
    VimBind(windmove_panel_swap_down, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_J, KeyCode_Shift));
    VimBind(windmove_panel_up, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_K));
    VimBind(windmove_panel_swap_up, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_K, KeyCode_Shift));
    VimBind(windmove_panel_left, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_H));
    VimBind(windmove_panel_swap_left, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_H, KeyCode_Shift));
    VimBind(windmove_panel_right, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_L));
    VimBind(windmove_panel_swap_right, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_L, KeyCode_Shift));
    VimBind(close_panel, vim_leader, vim_key(KeyCode_W), vim_key(KeyCode_D));
    
    VimBind(vim_toggle_line_comment_range_indent_style,          vim_leader, vim_key(KeyCode_C), vim_key(KeyCode_C));
    
    VimBind(vim_enter_normal_mode_escape,                        vim_key(KeyCode_Escape));
    VimBind(vim_isearch_word_under_cursor,                       vim_key(KeyCode_8, KeyCode_Shift));
    VimBind(vim_reverse_isearch_word_under_cursor,               vim_key(KeyCode_3, KeyCode_Shift));
    VimBind(vim_repeat_command,                                  vim_key(KeyCode_Period));
    VimBind(vim_move_line_up,                                    vim_key(KeyCode_K, KeyCode_Alt));
    VimBind(vim_move_line_down,                                  vim_key(KeyCode_J, KeyCode_Alt));
    VimBind(vim_isearch,                                         vim_key(KeyCode_ForwardSlash));
    VimBind(vim_isearch_backward,                                vim_key(KeyCode_ForwardSlash, KeyCode_Shift));
    VimBind(vim_isearch_repeat_forward,                          vim_key(KeyCode_N));
    VimBind(vim_isearch_repeat_backward,                         vim_key(KeyCode_N, KeyCode_Shift));
    VimBind(noh,                                                 vim_leader, vim_key(KeyCode_N));
    VimBind(goto_next_jump,                                      vim_key(KeyCode_N, KeyCode_Control));
    VimBind(goto_prev_jump,                                      vim_key(KeyCode_P, KeyCode_Control));
    VimBind(vim_undo,                                            vim_key(KeyCode_U));
    VimBind(vim_redo,                                            vim_key(KeyCode_R, KeyCode_Control));
    VimBind(command_lister,                                      vim_key(KeyCode_Semicolon, KeyCode_Shift));
    VimBind(if_read_only_goto_position,                          vim_key(KeyCode_Return));
    VimBind(if_read_only_goto_position_same_panel,               vim_key(KeyCode_Return, KeyCode_Shift));
    
    //
    // Visual Vim Map
    //
    
    VimSelectMap(vim_map_visual);
    VimAddParentMap(vim_map_normal);
    VimAddParentMap(vim_map_text_objects);
    
    VimBind(vim_enter_visual_insert_mode,                        vim_key(KeyCode_I, KeyCode_Shift))->flags |= VimBindingFlag_TextCommand;
    VimBind(vim_enter_visual_append_mode,                        vim_key(KeyCode_A, KeyCode_Shift))->flags |= VimBindingFlag_TextCommand;
    
    VimBind(vim_lowercase,                                       vim_key(KeyCode_U));
    VimBind(vim_uppercase,                                       vim_key(KeyCode_U, KeyCode_Shift));
    
    VimBind(vim_isearch_selection,                               vim_key(KeyCode_ForwardSlash));
    VimBind(vim_reverse_isearch_selection,                       vim_key(KeyCode_ForwardSlash, KeyCode_Shift));
}
void custom_layer_init(Application_Links *app) {
    Thread_Context *tctx = get_thread_context(app);
    
    // NOTE(allen): setup for default framework
    default_framework_init(app);
    
    // NOTE(allen): default hooks and command maps
    set_all_default_hooks(app);
    mapping_init(tctx, &framework_mapping);
    
    setup_default_mapping(&framework_mapping, mapid_global, mapid_file,
                          mapid_code);
    vim_init(app);
    vim_set_default_hooks(app);
    custom_setup_default_mapping(app, &framework_mapping, vim_key(KeyCode_Space));
    set_custom_hook(app, HookID_RenderCaller, custom_render_caller);
}

#endif // FCODER_DEFAULT_BINDINGS
// BOTTOM
