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

#ifndef YY_STACK_DESCRIPTION_STACK_DESCRIPTION_PARSER_H_INCLUDED
# define YY_STACK_DESCRIPTION_STACK_DESCRIPTION_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int stack_description_debug;
#endif
/* "%code requires" blocks.  */
#line 40 "stack_description_parser.y" /* yacc.c:1909  */

    #include "types.h"

    #include "material.h"
    #include "die.h"
    #include "stack_element.h"
    #include "inspection_point.h"
    #include "stack_description.h"

    typedef void* yyscan_t;

#line 56 "stack_description_parser.h" /* yacc.c:1909  */

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    _2RM = 258,
    _4RM = 259,
    AMBIENT = 260,
    AVERAGE = 261,
    BOTTOM = 262,
    CAPACITY = 263,
    CELL = 264,
    CHANNEL = 265,
    CHIP = 266,
    COEFFICIENT = 267,
    CONDUCTIVITY = 268,
    CONVENTIONAL = 269,
    COOLANT = 270,
    DARCY = 271,
    DIAMETER = 272,
    DIE = 273,
    DIMENSIONS = 274,
    DISTRIBUTION = 275,
    FINAL = 276,
    FIRST = 277,
    FLOORPLAN = 278,
    FLOW = 279,
    HEAT = 280,
    HEIGHT = 281,
    INCOMING = 282,
    INITIAL_ = 283,
    INLINE = 284,
    LAST = 285,
    LAYER = 286,
    LENGTH = 287,
    MATERIAL = 288,
    MAXIMUM = 289,
    MICROCHANNEL = 290,
    MINIMUM = 291,
    OUTPUT = 292,
    PIN = 293,
    PINFIN = 294,
    PITCH = 295,
    RATE = 296,
    SIDE = 297,
    SINK = 298,
    SLOT = 299,
    SOLVER = 300,
    SOURCE = 301,
    STACK = 302,
    STAGGERED = 303,
    STATE = 304,
    STEADY = 305,
    STEP = 306,
    TCELL = 307,
    TCOOLANT = 308,
    TEMPERATURE = 309,
    TFLP = 310,
    TFLPEL = 311,
    THERMAL = 312,
    TMAP = 313,
    TOP = 314,
    TRANSFER = 315,
    TRANSIENT = 316,
    VELOCITY = 317,
    VOLUMETRIC = 318,
    WALL = 319,
    WIDTH = 320,
    DVALUE = 321,
    IDENTIFIER = 322,
    PATH = 323
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 55 "stack_description_parser.y" /* yacc.c:1909  */

    double                double_v ;
    String_t              string ;
    Material             *material_p ;
    Coolant_t             coolant_v ;
    ChannelModel_t        channel_model_v ;
    Die                  *die_p ;
    Layer                *layer_p ;
    StackElement         *stack_element_p ;
    InspectionPoint      *inspection_point_p ;
    OutputInstant_t       output_instant_v ;
    OutputQuantity_t      output_quantity_v ;

#line 151 "stack_description_parser.h" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int stack_description_parse (StackDescription *stkd, Analysis         *analysis, yyscan_t          scanner);

#endif /* !YY_STACK_DESCRIPTION_STACK_DESCRIPTION_PARSER_H_INCLUDED  */
