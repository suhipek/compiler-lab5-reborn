#include <iostream>
#include <string.h>
#include <unistd.h>
#include "Ast.h"
using namespace std;

Ast ast;
extern FILE *yyin;
extern FILE *yyout;

int yyparse();

char outfile[256] = "a.out";
bool dump_tokens;
bool dump_ast;
extern int yylineno;

int main(int argc, char *argv[])
{
    dump_tokens = dump_ast = false;
    yyin = fopen("sysyruntimelibrary/sylib_def.h", "r"); // 链接sysy运行库
    yyparse();
    fclose(yyin);
    yylineno = 1; // 重置行号
    int opt;
    while ((opt = getopt(argc, argv, "ato:")) != -1)
    {
        switch (opt)
        {
        case 'o':
            strcpy(outfile, optarg);
            break;
        case 'a':
            dump_ast = true;
            break;
        case 't':
            dump_tokens = true;
            break;
        default:
            fprintf(stderr, "Usage: %s [-o outfile] infile\n", argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }
    if (optind >= argc)
    {
        fprintf(stderr, "no input file\n");
        exit(EXIT_FAILURE);
    }
    if (!(yyin = fopen(argv[optind], "r")))
    {
        fprintf(stderr, "%s: No such file or directory\nno input file\n", argv[optind]);
        exit(EXIT_FAILURE);
    }
    if (!(yyout = fopen(outfile, "w")))
    {
        fprintf(stderr, "%s: fail to open output file\n", outfile);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Processing %s \n", argv[optind]);
    yyparse();
    if(dump_ast)
        ast.output();
    return 0;
}
