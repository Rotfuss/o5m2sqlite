#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "o5mreader.c"
#include <sqlite3.h>



#define db_setup_statements "CREATE TABLE nodes (id INTEGER PRIMARY KEY,lat REAL CHECK ( lat <= 90 AND lat >= -90 ),lon REAL CHECK ( lon <= 180 AND lon >= -180 ));\n"\
"CREATE TABLE node_tags (node_id INTEGER REFERENCES nodes ( id ),key TEXT,value TEXT,UNIQUE ( node_id, key, value ));\n"\
        "CREATE TABLE ways (id INTEGER PRIMARY KEY);\n"\
        "CREATE TABLE way_tags (way_id INTEGER REFERENCES ways ( id ),key TEXT,value TEXT,UNIQUE ( way_id, key, value ));\n"\
        "CREATE TABLE way_nodes (way_id INTEGER REFERENCES ways ( id ),local_order INTEGER,node_id INTEGER REFERENCES nodes ( id ),UNIQUE ( way_id, local_order, node_id ));\n"\
        "CREATE TABLE relations (id INTEGER PRIMARY KEY);\n"\
        "CREATE TABLE relation_tags (relation_id INTEGER REFERENCES relations ( id ),key TEXT,value TEXT,UNIQUE ( relation_id, key, value ));\n"\
        "CREATE TABLE relation_members (relation_id INTEGER REFERENCES relations ( id ),type TEXT CHECK ( type IN (\"node\", \"way\", \"relation\")),ref INTEGER,role TEXT,local_order INTEGER);\n"
#define db_create_indexes "CREATE INDEX nodes_lat ON nodes ( lat );"\
        "CREATE INDEX nodes_lon ON nodes ( lon );\n"\
        "CREATE INDEX node_tags_node_id ON node_tags ( node_id );\n"\
        "CREATE INDEX node_tags_key ON node_tags ( key );\n"\
        "CREATE INDEX way_tags_way_id ON way_tags ( way_id );\n"\
        "CREATE INDEX way_tags_key ON way_tags ( key );\n"\
        "CREATE INDEX way_nodes_way_id ON way_nodes ( way_id );\n"\
        "CREATE INDEX way_nodes_node_id ON way_nodes ( node_id );\n"\
        "CREATE INDEX relation_tags_relation_id ON relation_tags ( relation_id );\n"\
        "CREATE INDEX relation_tags_key ON relation_tags ( key );\n"\
        "CREATE INDEX relation_members_relation_id ON relation_members ( relation_id );\n"\
        "CREATE INDEX relation_members_type ON relation_members ( type, ref );"
        
#define ins_node "insert into nodes (id,lat,lon) values (?,?,?);"
#define ins_node_tag "insert into node_tags (node_id,key,value) values (?,?,?);"
#define ins_way "insert into ways (id) values (?);"
#define ins_way_tag "insert into way_tags (way_id,key,value) values (?,?,?);"
#define ins_way_node "insert into way_nodes (way_id,node_id) values (?,?);"
#define ins_rel "insert into relations (id) values (?);"
#define ins_rel_tag "insert into relation_tags (relation_id,key,value) values (?,?,?);"
#define ins_rel_member "insert into relation_members (relation_id,type,ref,role) values (?,?,?,?);"


#define help "o5m2sqlite converts openstreetmap data to sqlite database files.\nInput file must be a binary file in the o5m format.\n\nUsage:\no5m2sqlite in.o5m out.sqlite\tconvert in.o5m to out.sqlite\no5m2sqlite --schema\t\tshow the resulting sqlite database schema.\n\nTillmann Stuebler, 12 August 2016\n\n"




int main(int narg, char * arg[])
{
    // OSM
    O5mreader* reader;
    O5mreaderDataset ds;
    O5mreaderIterateRet ret, ret2;
    char *key, *val;
    int64_t nodeId;
    int64_t refId;
    uint8_t type;
    char *role;
    FILE * f;
    
    // SQLITE
    sqlite3 *db;
    sqlite3_stmt *stmt_node, *stmt_node_tag, *stmt_way, *stmt_way_tag, *stmt_way_node, *stmt_rel, *stmt_rel_tag, *stmt_rel_member;
    int r;
    
    if((narg==2) && strcmp(arg[1],"--schema")==0) {
        printf("%s\n%s\n\n",db_setup_statements,db_create_indexes);
        return 0;
    }
    
    if(narg<3) {
        printf(help);
        return 0;
    }
    
    // open O5M file
    f = fopen(arg[1],"rb");
    if(f==NULL) {
        printf("Could not open o5m file!\n");
        return -2;
    }
    
    // open SQLITE database
    r=sqlite3_open(arg[2],&db);
    if(r != SQLITE_OK) {
        printf("Could not open sqlite3 database!\n");
        sqlite3_close(db);
        return -3;
    }
    
    sqlite3_exec(db,"PRAGMA synchronous = OFF",NULL,NULL,NULL);
    sqlite3_exec(db,"PRAGMA journal_mode = MEMORY",NULL,NULL,NULL);
    
    // prepare tables
    r=sqlite3_exec(db,db_setup_statements,NULL,NULL,NULL);
    if(r!=SQLITE_OK) {
        printf("Could not create tables!\n");
        sqlite3_close(db);
        return -4;
    }
    
    // prepare statements
    if ((sqlite3_prepare(db,ins_node,-1,&stmt_node,NULL)!=SQLITE_OK) || \
            (sqlite3_prepare(db,ins_node_tag,-1,&stmt_node_tag,NULL)!=SQLITE_OK) || \
            (sqlite3_prepare(db,ins_way,-1,&stmt_way,NULL)!=SQLITE_OK) || \
            (sqlite3_prepare(db,ins_way_tag,-1,&stmt_way_tag,NULL)!=SQLITE_OK) || \
            (sqlite3_prepare(db,ins_way_node,-1,&stmt_way_node,NULL)!=SQLITE_OK) || \
            (sqlite3_prepare(db,ins_rel,-1,&stmt_rel,NULL)!=SQLITE_OK) || \
            (sqlite3_prepare(db,ins_rel_tag,-1,&stmt_rel_tag,NULL)!=SQLITE_OK) || \
            (sqlite3_prepare(db,ins_rel_member,-1,&stmt_rel_member,NULL)!=SQLITE_OK)) {
        printf("Could not prepare statememts!\n");
        sqlite3_close(db);
        return -5;
    }
    
    o5mreader_open(&reader,f);
    
    // iterate over the o5m file entries
    while( (ret = o5mreader_iterateDataSet(reader, &ds)) == O5MREADER_ITERATE_RET_NEXT ) {
        switch ( ds.type ) {
            // Data set is node
            case O5MREADER_DS_NODE:
                // Could do something with ds.id, ds.lon, ds.lat here, lon and lat are ints in 1E+7 * degree units
                sqlite3_bind_int64(stmt_node,1,ds.id);
                sqlite3_bind_double(stmt_node,2,ds.lat/1E7);
                sqlite3_bind_double(stmt_node,3,ds.lon/1E7);
                if(sqlite3_step(stmt_node)==SQLITE_DONE) sqlite3_reset(stmt_node);
                else {
                    printf("could not insert node.\n");
                    sqlite3_close(db);
                    return -6;
                }
                
                sqlite3_bind_int64(stmt_node_tag,1,ds.id);
                // Node tags iteration, can be omited
                while ( (ret2 = o5mreader_iterateTags(reader,&key,&val)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with tag key and val
                    sqlite3_bind_text(stmt_node_tag,2,key,-1,NULL);
                    sqlite3_bind_text(stmt_node_tag,3,val,-1,NULL);
                    if(sqlite3_step(stmt_node_tag)==SQLITE_DONE) sqlite3_reset(stmt_node_tag);
                    else {
                        printf("could not insert node tag.\n");
                        sqlite3_close(db);
                        return -7;
                    }
                }
                break;
                
                // Data set is way
            case O5MREADER_DS_WAY:
                // Could do something with ds.id
                sqlite3_bind_int64(stmt_way,1,ds.id);
                if(sqlite3_step(stmt_way)==SQLITE_DONE) sqlite3_reset(stmt_way);
                else {
                    printf("could not insert way.\n");
                    sqlite3_close(db);
                    return -8;
                }
                
                sqlite3_bind_int64(stmt_way_node,1,ds.id);
                // Nodes iteration, can be omited
                while ( (ret2 = o5mreader_iterateNds(reader,&nodeId)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with nodeId
                    sqlite3_bind_int64(stmt_way_node,2,nodeId);
                    if(sqlite3_step(stmt_way_node)==SQLITE_DONE) sqlite3_reset(stmt_way_node);
                    else {
                        printf("could not insert way node.\n");
                        sqlite3_close(db);
                        return -9;
                    }
                }
                
                sqlite3_bind_int64(stmt_way_tag,1,ds.id);
                // Way taga iteration, can be omited
                while ( (ret2 = o5mreader_iterateTags(reader,&key,&val)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with tag key and val
                    sqlite3_bind_text(stmt_way_tag,2,key,-1,NULL);
                    sqlite3_bind_text(stmt_way_tag,3,val,-1,NULL);
                    if(sqlite3_step(stmt_way_tag)==SQLITE_DONE) sqlite3_reset(stmt_way_tag);
                    else {
                        printf("could not insert way tag.\n");
                        sqlite3_close(db);
                        return -10;
                    }
                }
                break;
                
                // Data set is rel
            case O5MREADER_DS_REL:
                // Could do something with ds.id
                sqlite3_bind_int64(stmt_rel,1,ds.id);
                if(sqlite3_step(stmt_rel)==SQLITE_DONE) sqlite3_reset(stmt_rel);
                else {
                    printf("could not insert rel.\n");
                    sqlite3_close(db);
                    return -11;
                }
                
                sqlite3_bind_int64(stmt_rel_member,1,ds.id);
                // Refs iteration, can be omited
                while ( (ret2 = o5mreader_iterateRefs(reader,&refId,&type,&role)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with refId (way or node or rel id depends on type), type and role
                    
                    switch(type) {
                        case O5MREADER_DS_NODE:
                            sqlite3_bind_text(stmt_rel_member,2,"node",-1,NULL);
                            break;
                        case O5MREADER_DS_WAY:
                            sqlite3_bind_text(stmt_rel_member,2,"way",-1,NULL);
                            break;
                        case O5MREADER_DS_REL:
                            sqlite3_bind_text(stmt_rel_member,2,"relation",-1,NULL);
                            break;
                        default:
                            sqlite3_bind_text(stmt_rel_member,2,"",-1,NULL);
                            break;
                    }
                    sqlite3_bind_int64(stmt_rel_member,3,refId);
                    sqlite3_bind_text(stmt_rel_member,4,role,-1,NULL);
                    
                    if(sqlite3_step(stmt_rel_member)==SQLITE_DONE) sqlite3_reset(stmt_rel_member);
                    else {
                        printf("could not insert rel member.\n");
                        sqlite3_close(db);
                        return -12;
                    }
                }
                
                sqlite3_bind_int64(stmt_rel_tag,1,ds.id);
                // Relation tags iteration, can be omited
                while ( (ret2 = o5mreader_iterateTags(reader,&key,&val)) == O5MREADER_ITERATE_RET_NEXT  ) {
                    // Could do something with tag key and val
                    sqlite3_bind_text(stmt_rel_tag,2,key,-1,NULL);
                    sqlite3_bind_text(stmt_rel_tag,3,val,-1,NULL);
                    
                    if(sqlite3_step(stmt_rel_tag)==SQLITE_DONE) sqlite3_reset(stmt_rel_tag);
                    else {
                        printf("could not insert rel tag.\n");
                        sqlite3_close(db);
                        return -13;
                    }
                }
                break;
        } // end of switch-case
    } // end of o5m elements iteration
    
    // close o5m file
    fclose(f);
    
    // create sqlite indexes
    r=sqlite3_exec(db,db_create_indexes,NULL,NULL,NULL);
    if(r!=SQLITE_OK) {
        printf("Could not create indexes!\n");
        sqlite3_close(db);
        return -7;
    }
    
    // close sqlite database
    sqlite3_close(db);
    
    return 0;
}