struct Project_Line
{
    String_Const_u8 name;
    String_Const_u8 dir;
    String_Const_u8 project_file_name;
};

struct Project_Line_Array
{
    Project_Line *project_files;
    i32 count;
};

struct Project_File_Result
{
    b32 loaded;
    
    String_Const_u8 dir;
    Project_Line_Array project_array;
};

struct Project_File_Parse_Result
{
    Config *parsed;
    Project_File_Result *project_file;
};

struct Project_Lister_Result
{
    b32 success;
    i32 index;
};

/////////////////////////
global Project_File_Result *project_file = {};
global String_Const_u8 project_file_path;

function Project_File_Result *
parse_project_file__config_data__version_1(Application_Links *app, Arena *arena, String_Const_u8 file_dir, Config *parsed)
{
    Project_File_Result *project = push_array_zero(arena, Project_File_Result, 1);
    
    project->dir = push_string_copy(arena, file_dir);
    //load proj
    {
        Config_Compound *compound = 0;
        if (config_compound_var(parsed, "projects", 0, &compound))
        {
            Config_Get_Result_List list = typed_compound_array_reference_list(arena, parsed, compound);
            
            project->project_array.project_files = push_array(arena, Project_Line, list.count);
            project->project_array.count = list.count;
            
            Project_Line *dst = project->project_array.project_files;
            
            for (Config_Get_Result_Node *node = list.first;
                 node != 0;
                 node = node->next, ++dst)
            {
                Config_Compound *src = node->result.compound;
                block_zero_struct(dst);
                u8 *pos = node->result.pos;
                b32 can_emit_command = true;
                String_Const_u8 name = {};
                String_Const_u8 dir = {};
                String_Const_u8 project_file_name = {};
                
                //extract name
                if (!config_compound_string_member(parsed, src, "name", 0, &name))
                {
                    can_emit_command = false;
                    config_add_error(arena, parsed, pos, "a command must have a string type name member");
                    goto finish_command;
                }
                
                if (!config_compound_string_member(parsed, src, "dir", 0, &dir))
                {
                    can_emit_command = false;
                    config_add_error(arena, parsed, pos, "a command must have a string type name member");
                    goto finish_command;
                }
                
                if (!config_compound_string_member(parsed, src, "project_file_name", 0, &project_file_name))
                {
                    can_emit_command = false;
                    config_add_error(arena, parsed, pos, "a command must have a string type name member");
                    goto finish_command;
                }
                
                dst->name = push_string_copy(arena, name);
                dst->dir = push_string_copy(arena, dir);
                dst->project_file_name = push_string_copy(arena, project_file_name);
                
                finish_command:;
            }
        }
    }
    project->loaded = true;
    return (project);
}

function Project_File_Result *
parse_project__data(Application_Links *app, Arena *arena, String_Const_u8 file_dir, Config *parsed)
{
    i32 version = 1;
    
    Project_File_Result *result = 0;
    switch (version)
    {
        case 1:
        {
            result = parse_project_file__config_data__version_1(app, arena, file_dir, parsed);
        }
        break;
    }
    
    return (result);
}

function Project_File_Parse_Result
parse_project_file(Application_Links *app, Arena *arena)
{
    if(project_file_path.size == 0)
    {
        project_file_path = push_hot_directory(app, arena);
    }
    File_Name_Data dump = dump_file_search_up_path(app, arena, project_file_path, string_u8_litexpr("project.file"));
    Project_File_Parse_Result result = {};
    Token_Array array = token_array_from_text(app, arena, SCu8(dump.data));
    if (array.tokens != 0)
    {
        result.parsed = config_parse(app, arena, dump.file_name, SCu8(dump.data), array);
        if (result.parsed != 0)
        {
            String_Const_u8 project_root = string_remove_last_folder(dump.file_name);
            result.project_file = parse_project__data(app, arena, project_root, result.parsed);
            project_file = result.project_file;
        }
    }
    return (result);
}
/////////////////////////

function void
set_project(Application_Links *app, Arena *arena, i32 index)
{
    Project_Line proj = project_file->project_array.project_files[index];
    File_Name_Data dump = dump_file_search_up_path(app, arena, proj.dir, proj.project_file_name);
    set_current_project_from_data(app, proj.project_file_name,
                                  dump.data, proj.dir);
}

function Project_Lister_Result
get_projects_from_file(Application_Links *app, Arena *arena, String_Const_u8 query)
{
    Project_Lister_Result result = {};
    
    Lister_Block lister(app, arena);
    lister_set_query(lister, query);
    lister_set_default_handlers(lister);
    
    //load file
    Project_File_Parse_Result project_file_result = parse_project_file(app, arena);
    Project_Line *project_line = project_file->project_array.project_files;
    i32 count = project_file_result.project_file->project_array.count;
    for (i32 i = 0; i < count; i += 1, project_line += 1)
    {
        lister_add_item(lister, project_line->name, project_line->dir, IntAsPtr(i), 0);
    }
    
    Lister_Result l_result = run_lister(app, lister);
    if (!l_result.canceled)
    {
        result.success = true;
        result.index = (i32)PtrAsInt(l_result.user_data);
    }
    
    return (result);
}

function Project_Lister_Result
get_projects_from_file(Application_Links *app, Arena *arena, char *query)
{
    return (get_projects_from_file(app, arena, SCu8(query)));
}

CUSTOM_COMMAND_SIG(project_lister)
CUSTOM_DOC("Open a lister of all projects")
{
    Scratch_Block scratch(app);
    Project_Lister_Result proj =
        get_projects_from_file(app, scratch, "Project:");
    if (proj.success)
    {
        set_project(app, scratch, proj.index);
    }
}