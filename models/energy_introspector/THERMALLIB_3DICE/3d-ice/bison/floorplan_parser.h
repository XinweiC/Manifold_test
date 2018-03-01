/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_FLOORPLAN_FLOORPLAN_PARSER_H_INCLUDED
# define YY_FLOORPLAN_FLOORPLAN_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int floorplan_debug;
#endif
/* "%code requires" blocks.  */
#line 40 "floorplan_parser.y" /* yacc.c:1909  */

    #include "types.h"
    #include "floorplan_element.h"
    #include "ic_element.h"
    #include "powers_queue.h"
    #include "floorplan.h"

    typedef void* yyscan_t;

#line 54 "floorplan_parser.h" /* yacc.c:1909  */

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    DIMENSION = 258,
    POSITION = 259,
    POWER = 260,
    RECTANGLE = 261,
    VALUES = 262,
    DVALUE = 263,
    IDENTIFIER = 264
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 51 "floorplan_parser.y" /* yacc.c:1909  */

    Power_t           power_value ;
    String_t          identifier ;
    ICElement        *p_icelement ;
    FloorplanElement *p_floorplan_element ;
    PowersQueue      *p_powers_queue ;

#line 84 "floorplan_parser.h" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int floorplan_parse (Floorplan*  floorplan, Dimensions* dimensions, yyscan_t    scanner);

#endif /* !YY_FLOORPLAN_FLOORPLAN_PARSER_H_INCLUDED  */
