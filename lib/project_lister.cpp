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

struct Project_File
{
    Project_Line_Array project_array;
};

struct Project_Lister_Result
{
    b32 success;
    i32 index;
};
/////////////////////////
#include "external/SimpleJSON/src/JSON.h"

global Project_File project_file = {};

function void add_project_to_file(Application_Links *app, Project *project, Arena *arena)
{
    Project_Line_Array old = project_file.project_array;
    project_file.project_array.count = old.count + 1;
    project_file.project_array.project_files = push_array(arena, Project_Line, old.count + 1);
    
    Project_Line *project_line = project_file.project_array.project_files;
    for(int i = 0; i < old.count; i++)
    {
        project_line->name = old.project_files[i].name;
        project_line->dir = old.project_files[i].dir;
        project_line->project_file_name = old.project_files[i].project_file_name;
    }
    if(project->loaded)
    {
        project_line[old.count].name = project->name;
        project_line[old.count].dir = project->dir;
        project_line[old.count].project_file_name = SCu8("project.4coder");
    }
}

function void parse_project_file(Application_Links *app, Arena *arena)
{
    // TODO(Sam): Actually read from a file
    
    project_file.project_array.count = 1;
    project_file.project_array.project_files = push_array(arena, Project_Line, 1);
    
    Project_Line *project_line = project_file.project_array.project_files;
    for(int i = 0; i < 1; i++)
    {
        project_line->name = SCu8("4coder");
        project_line->dir = SCu8("/home/sam/.bin/4coder");
        project_line->project_file_name = SCu8("project.4coder");
    }
}

function void
set_project(Application_Links *app, Arena *arena, i32 index)
{
    Project_Line proj = project_file.project_array.project_files[index];
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
    parse_project_file(app, arena);
    Project_Line *project_line = project_file.project_array.project_files;
    i32 count = project_file.project_array.count;
    for(i32 i = 0; i < count; i++)
    {
        lister_add_item(lister, project_line->name, project_line->dir, IntAsPtr(i), 0);
    }
    
    Lister_Result l_result = run_lister(app, lister);
    if(!l_result.canceled)
    {
        result.success = true;
        result.index = (i32)PtrAsInt(l_result.user_data);
    }
    
    return (result);
}

function Project_Lister_Result
get_projects_from_file(Application_Links *app, Arena *arena, char *query){
    return(get_projects_from_file(app, arena, SCu8(query)));
}

CUSTOM_COMMAND_SIG(project_lister)
CUSTOM_DOC("Open a lister of all projects")
{
    Scratch_Block scratch(app);
    Project_Lister_Result proj =
        get_projects_from_file(app, scratch, "Project:");
    if (proj.success){
        set_project(app, scratch, proj.index);
    }
}

CUSTOM_COMMAND_SIG(add_project_to_master)
CUSTOM_DOC("Adds loaded project into master file")
{
    Scratch_Block scratch(app);
    add_project_to_file(app, &current_project, scratch);
}