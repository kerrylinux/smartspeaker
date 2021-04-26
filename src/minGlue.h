#include <stdio.h>

#define INI_FILETYPE                    FILE*
#define ini_openread(filename,file)     ((*(file) = fopen((filename),"rb")) != NULL)
#define ini_openwrite(filename,file)    ((*(file) = fopen((filename),"wb")) != NULL)
#define ini_openrewrite(filename,file)  ((*(file) = fopen((filename),"r+b")) != NULL)
#define ini_close(file)                 (fclose(*(file)) == 0)
#define ini_read(buffer,size,file)      (fgets((buffer),(size),*(file)) != NULL)
#define ini_write(buffer,file)          (fputs((buffer),*(file)) >= 0)
#define ini_rename(source,dest)         (rename((source), (dest)) == 0)
#define ini_remove(filename)            (remove(filename) == 0)
#define INI_FILEPOS                     long int
#define ini_tell(file,pos)              (*(pos) = ftell(*(file)))
#define ini_seek(file,pos)              (fseek(*(file), *(pos), SEEK_SET) == 0)
#define INI_REAL                        float
#define ini_ftoa(string,value)          sprintf((string),"%f",(value))
#define ini_atof(string)                (INI_REAL)strtod((string),NULL)
