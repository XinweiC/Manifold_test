/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         stack_description_parse
#define yylex           stack_description_lex
#define yyerror         stack_description_error
#define yydebug         stack_description_debug
#define yynerrs         stack_description_nerrs


/* Copy the first part of user declarations.  */

#line 73 "stack_description_parser.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* In a future release of Bison, this section will be replaced
   by #include "stack_description_parser.h".  */
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
#line 40 "stack_description_parser.y" /* yacc.c:355  */

    #include "types.h"

    #include "material.h"
    #include "die.h"
    #include "stack_element.h"
    #include "inspection_point.h"
    #include "stack_description.h"

    typedef void* yyscan_t;

#line 115 "stack_description_parser.c" /* yacc.c:355  */

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
#line 55 "stack_description_parser.y" /* yacc.c:355  */

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

#line 210 "stack_description_parser.c" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int stack_description_parse (StackDescription *stkd, Analysis         *analysis, yyscan_t          scanner);

#endif /* !YY_STACK_DESCRIPTION_STACK_DESCRIPTION_PARSER_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 224 "stack_description_parser.c" /* yacc.c:358  */
/* Unqualified %code blocks.  */
#line 70 "stack_description_parser.y" /* yacc.c:359  */

    #include "analysis.h"
    #include "channel.h"
    #include "conventional_heat_sink.h"
    #include "dimensions.h"
    #include "floorplan_element.h"
    #include "floorplan.h"
    #include "layer.h"
    #include "macros.h"
    #include "stack_description.h"

    #include "../flex/stack_description_scanner.h"

    void stack_description_error

        (StackDescription *stack, Analysis *analysis,
         yyscan_t scanner, String_t message) ;

    static char error_message [100] ;

    static CellDimension_t first_wall_length ;
    static CellDimension_t last_wall_length ;
    static CellDimension_t wall_length ;
    static CellDimension_t channel_length ;
    static Quantity_t      num_channels ;
    static Quantity_t      num_dies ;

#line 254 "stack_description_parser.c" /* yacc.c:359  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  6
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   310

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  28
/* YYNRULES -- Number of rules.  */
#define YYNRULES  55
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  315

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   323

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      72,    73,     2,     2,    71,     2,    74,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    69,    70,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   220,   220,   237,   243,   265,   290,   294,   316,   320,
     376,   428,   482,   488,   500,   506,   516,   517,   521,   522,
     526,   527,   537,   540,   549,   551,   555,   596,   601,   623,
     721,   802,   896,   909,   940,   996,  1024,  1097,  1108,  1151,
    1161,  1165,  1171,  1176,  1186,  1247,  1320,  1417,  1460,  1535,
    1536,  1537,  1543,  1544,  1545,  1546
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "\"keyword 2rm\"", "\"keyword 4rm\"",
  "\"keyword ambient\"", "\"keyword average\"", "\"keyword bottom\"",
  "\"keyword capacity\"", "\"keyword cell\"", "\"keyword channel\"",
  "\"keyword chip\"", "\"keyword coefficient\"",
  "\"keyword conductivity\"", "\"keyword conventional\"",
  "\"keyword coolant\"", "\"keyword darcy\"", "\"keyword diameter\"",
  "\"keyword die\"", "\"keyword dimensions\"", "\"keyword distribution\"",
  "\"keyword final\"", "\"keyword first\"", "\"keyword floorplan\"",
  "\"keyword flow\"", "\"keyword heat\"", "\"keyword height\"",
  "\"keyword incoming\"", "\"keyword initial\"", "\"keyword inline\"",
  "\"keyword last\"", "\"keyword layer\"", "\"keyword length\"",
  "\"keyword material\"", "\"keyword maximum\"",
  "\"keyword microchannel\"", "\"keyword minimum\"", "\"keyword output\"",
  "\"keyword pin\"", "\"keyword pinfin\"", "\"keyword pitch\"",
  "\"keyword rate\"", "\"keyword side\"", "\"keywork sink\"",
  "\"keyword slot\"", "\"keyword solver\"", "\"keyword source\"",
  "\"keyword stack\"", "\"keyword staggered\"", "\"keyword state\"",
  "\"keyword steady\"", "\"keyword step\"", "\"keyword T\"",
  "\"keyword Tcoolant\"", "\"keyword temperature\"", "\"keyword Tflp\"",
  "\"keyword Tflpel\"", "\"keyword thermal\"", "\"keyword Tmap\"",
  "\"keyword top\"", "\"keyword transfer\"", "\"keyword transient\"",
  "\"keyword velocity\"", "\"keywork volumetric\"", "\"keyword wall\"",
  "\"keyword width\"", "\"double value\"", "\"identifier\"",
  "\"path to file\"", "':'", "';'", "','", "'('", "')'", "'.'", "$accept",
  "stack_description_file", "materials_list", "material",
  "conventional_heat_sink", "microchannel",
  "coolant_heat_transfer_coefficients_4rm",
  "coolant_heat_transfer_coefficients_2rm", "first_wall_length",
  "last_wall_length", "distribution", "layers_list", "layer",
  "source_layer", "layer_content", "dies_list", "die", "dimensions",
  "stack", "stack_elements", "stack_element", "solver",
  "initial_temperature", "inspection_points", "inspection_points_list",
  "inspection_point", "maxminavg", "when", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,    58,
      59,    44,    40,    41,    46
};
# endif

#define YYPACT_NINF -180

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-180)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     -13,   -40,    29,   -10,  -180,   -29,  -180,    17,  -180,   -23,
     -14,     1,    10,   -22,    27,    33,   -21,   -20,   -19,    25,
     -15,     0,  -180,   -12,    28,    30,    31,   -11,    -9,    -8,
    -180,    11,    -7,    -1,    -2,     2,    -5,  -180,    51,     3,
      21,     4,    57,     5,     6,    32,   -25,    39,    -3,     8,
      45,    49,    12,    69,    70,    64,    16,    16,  -180,  -180,
      18,    19,    20,    22,    -3,  -180,   -39,    34,    46,    77,
      23,    58,    59,    26,    35,  -180,  -180,    63,    24,    36,
      37,    41,  -180,    38,    47,    43,    42,  -180,    44,    91,
      48,    50,    52,    53,    40,  -180,    74,    54,  -180,    60,
      55,   -17,    61,    65,    62,    66,    75,  -180,    67,    56,
      68,    71,  -180,    72,    73,    76,    78,    79,   -17,  -180,
    -180,    80,    83,    85,    87,    82,    84,  -180,    86,    88,
      89,    90,    92,    93,  -180,    94,    96,    97,    95,   103,
    -180,    99,    98,   100,   101,   102,   104,  -180,   107,   108,
     109,   105,   110,   111,   113,   114,   116,   117,   118,   119,
     115,   112,  -180,   120,   121,   122,   123,   124,   106,    81,
     138,   125,   131,    -4,    -4,   130,   -18,   126,   129,   136,
     133,   -24,   137,   132,  -180,  -180,  -180,   124,   124,   134,
    -180,  -180,  -180,   139,   140,   135,   142,   144,  -180,  -180,
     141,   146,   145,   128,   143,    -4,  -180,   147,   148,   152,
     153,   149,   150,   124,   151,   154,   124,   175,   155,   156,
     159,   171,  -180,   157,  -180,  -180,   158,   186,  -180,   162,
     163,   160,   164,   165,   127,  -180,   200,   166,  -180,  -180,
     172,   194,   203,   167,   182,   177,   213,   174,   176,   204,
     226,   173,   178,   184,   183,   230,   232,   237,   225,   227,
     236,   190,   -49,   246,   195,   193,   233,   191,   189,   196,
     248,   238,   253,   197,  -180,   199,   -41,   256,   201,   258,
     251,   205,   202,   207,   206,   208,   243,   209,  -180,   211,
     260,   212,   223,   219,   264,   257,  -180,   217,   220,   261,
     231,   221,   216,   235,   224,  -180,   285,   228,   229,   234,
     239,  -180,   240,  -180,  -180
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     6,     3,     0,     1,     0,     4,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    27,     0,     0,     0,     0,     0,     0,     0,
      28,     0,     0,     0,     0,     0,     0,    22,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    23,    22,
       0,     0,     0,     0,    31,    32,     0,     0,    40,     0,
       0,     0,     0,     0,     0,    24,    25,    29,     0,     0,
       0,     0,    33,     0,     0,     0,     0,     2,     0,     0,
       0,     0,     0,     0,     0,    35,     0,     0,    37,     0,
       0,     0,     0,     0,     0,     0,     0,    26,     0,     0,
       0,     0,    39,     0,     0,     0,     0,     0,    41,    42,
       5,     0,     0,     0,     0,     0,     0,    34,     0,     0,
       0,     0,     0,     0,    43,     0,     0,     0,     0,     0,
      36,     0,     0,     0,     0,     0,     0,     7,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    38,     0,     0,     0,     0,    52,     0,    16,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      18,     0,     0,     0,    51,    49,    50,    52,    52,     0,
      55,    54,    53,     0,     0,     0,     0,     0,    20,    21,
       0,     0,     0,     0,     0,     0,    47,     0,     0,     0,
       0,     0,     0,    52,     0,     0,    52,     0,     0,     0,
       0,     0,    30,     0,    48,    45,     0,     0,    17,     0,
       0,     0,     0,     0,     0,    19,     0,     0,    44,    46,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    14,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    12,     0,
       0,     0,     0,     0,     0,     0,    15,     0,     0,     0,
       0,     0,     0,     0,     0,    10,     0,     0,     0,     0,
       0,    11,     0,     9,    13
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -180,  -180,  -180,   290,  -180,  -180,  -180,  -180,  -180,  -180,
    -180,   242,  -180,  -180,   241,  -180,   274,  -180,  -180,  -180,
     244,  -180,  -180,  -180,  -180,   179,  -174,  -179
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,     4,     9,    14,   260,   250,   180,   197,
     200,    46,    58,    59,    75,    21,    22,    31,    40,    64,
      65,    50,    68,    87,   118,   119,   187,   177
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint16 yytable[] =
{
     188,   281,   184,   190,     7,   198,    56,    61,   203,   204,
     267,    83,    12,    17,    18,    62,    13,   268,    20,    29,
       1,    57,    84,     1,   199,   282,   191,     5,    63,     6,
     185,   216,   186,   192,   223,   113,   114,   226,   115,   116,
      10,   117,    11,    15,    16,    20,    23,    19,    24,    25,
      26,    27,    28,    33,    32,    36,    34,    35,    39,    42,
      37,    38,    47,    41,    43,    45,    49,    51,    44,    52,
      55,    60,    48,    67,    69,    53,    54,    66,    70,    71,
      72,    73,    74,    86,    78,    88,    79,    80,    85,    81,
      90,    91,    92,    89,    56,    94,   103,   109,    99,     0,
       0,     0,    93,   179,    96,   108,    95,    97,    98,   100,
     102,   101,   151,   124,   104,     0,   105,     0,     0,   121,
       0,   110,   106,   107,   126,   112,   111,   138,   148,   149,
     141,   120,   122,   125,     0,     0,   123,   161,   127,     0,
       0,     0,   128,     0,   129,   130,   135,   136,   131,   137,
     132,   133,   139,   170,   140,   142,   143,   144,   181,   145,
     146,   150,   194,   196,   147,   152,     0,   208,   240,   153,
     178,   154,   155,   158,   159,   157,   156,   163,   171,   160,
     162,   164,   165,   166,   219,   167,   220,   221,   168,   169,
     227,   172,   173,   174,   175,   176,   182,   183,   189,   193,
     195,   214,   201,   202,   231,   205,   209,   207,   210,   206,
     234,   211,   212,   213,   218,   241,   215,   217,   244,   245,
     222,   224,   229,   247,   225,   228,   230,   237,   249,   253,
     232,   233,   235,   236,   238,   239,   242,   246,   243,   248,
     251,   254,   252,   255,   257,   259,   258,   261,   256,   262,
     263,   265,   264,   266,   269,   270,   271,   273,   272,   274,
     276,   278,   275,   277,   283,   285,   286,   284,   279,   280,
     292,   287,   288,   289,   291,   295,   290,   297,   298,   299,
     293,   294,   296,   301,   300,   304,   302,   306,   303,   307,
     308,   305,   309,     8,   310,    30,     0,   134,    76,   311,
     312,    77,     0,     0,     0,     0,     0,     0,    82,   313,
     314
};

static const yytype_int16 yycheck[] =
{
     174,    42,     6,    21,    14,    29,    31,    10,   187,   188,
      59,    50,    35,     3,     4,    18,    39,    66,    18,    19,
      33,    46,    61,    33,    48,    66,    44,    67,    31,     0,
      34,   205,    36,    51,   213,    52,    53,   216,    55,    56,
      69,    58,    25,    57,    43,    18,    13,    69,    69,    69,
      69,    26,    67,    25,    66,    66,    26,    26,    47,    60,
      69,    69,    11,    70,    66,    70,    45,    63,    66,    12,
      38,    32,    69,    28,    25,    70,    70,    69,    66,    10,
      10,    17,    66,    37,    66,     8,    67,    67,    54,    67,
      32,    32,    66,    70,    31,    71,     5,    23,    51,    -1,
      -1,    -1,    67,    22,    67,    65,    70,    66,    70,    66,
      66,    69,     9,    38,    66,    -1,    66,    -1,    -1,    54,
      -1,    67,    70,    70,    68,    70,    66,    40,    32,    32,
      44,    70,    70,    66,    -1,    -1,    70,    32,    70,    -1,
      -1,    -1,    71,    -1,    72,    72,    66,    64,    72,    64,
      72,    72,    70,    38,    70,    67,    67,    67,    20,    67,
      67,    66,    33,    30,    70,    66,    -1,    32,    41,    71,
      64,    71,    71,    66,    66,    71,    74,    66,    66,    70,
      70,    68,    68,    67,    32,    68,    33,    38,    70,    70,
      15,    71,    71,    71,    71,    71,    71,    66,    68,    73,
      64,    73,    65,    71,    33,    71,    64,    67,    64,    70,
      24,    70,    66,    68,    66,    15,    73,    70,    24,    16,
      70,    70,    66,    41,    70,    70,    67,    67,    15,    25,
      73,    73,    70,    70,    70,    70,    70,    70,    66,    62,
      66,    15,    66,    70,    60,    15,    63,    15,    70,    12,
      25,    15,    25,    63,     8,    60,    63,    66,    25,    70,
      12,     8,    66,    25,     8,     7,    15,    66,    71,    70,
      27,    66,    70,    66,    66,    15,    70,    54,    59,    15,
      71,    70,    70,    66,    27,    54,    66,    71,    27,    54,
      66,    70,     7,     3,    66,    21,    -1,   118,    57,    70,
      66,    59,    -1,    -1,    -1,    -1,    -1,    -1,    64,    70,
      70
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    33,    76,    77,    78,    67,     0,    14,    78,    79,
      69,    25,    35,    39,    80,    57,    43,     3,     4,    69,
      18,    90,    91,    13,    69,    69,    69,    26,    67,    19,
      91,    92,    66,    25,    26,    26,    66,    69,    69,    47,
      93,    70,    60,    66,    66,    70,    86,    11,    69,    45,
      96,    63,    12,    70,    70,    38,    31,    46,    87,    88,
      32,    10,    18,    31,    94,    95,    69,    28,    97,    25,
      66,    10,    10,    17,    66,    89,    89,    86,    66,    67,
      67,    67,    95,    50,    61,    54,    37,    98,     8,    70,
      32,    32,    66,    67,    71,    70,    67,    66,    70,    51,
      66,    69,    66,     5,    66,    66,    70,    70,    65,    23,
      67,    66,    70,    52,    53,    55,    56,    58,    99,   100,
      70,    54,    70,    70,    38,    66,    68,    70,    71,    72,
      72,    72,    72,    72,   100,    66,    64,    64,    40,    70,
      70,    44,    67,    67,    67,    67,    67,    70,    32,    32,
      66,     9,    66,    71,    71,    71,    74,    71,    66,    66,
      70,    32,    70,    66,    68,    68,    67,    68,    70,    70,
      38,    66,    71,    71,    71,    71,    71,   102,    64,    22,
      83,    20,    71,    66,     6,    34,    36,   101,   101,    68,
      21,    44,    51,    73,    33,    64,    30,    84,    29,    48,
      85,    65,    71,   102,   102,    71,    70,    67,    32,    64,
      64,    70,    66,    68,    73,    73,   101,    70,    66,    32,
      33,    38,    70,   102,    70,    70,   102,    15,    70,    66,
      67,    33,    73,    73,    24,    70,    70,    67,    70,    70,
      41,    15,    70,    66,    24,    16,    70,    41,    62,    15,
      82,    66,    66,    25,    15,    70,    70,    60,    63,    15,
      81,    15,    12,    25,    25,    15,    63,    59,    66,     8,
      60,    63,    25,    66,    70,    66,    12,    25,     8,    71,
      70,    42,    66,     8,    66,     7,    15,    66,    70,    66,
      70,    66,    27,    71,    70,    15,    70,    54,    59,    15,
      27,    66,    66,    27,    54,    70,    71,    54,    66,     7,
      66,    70,    66,    70,    70
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    75,    76,    77,    77,    78,    79,    79,    80,    80,
      80,    80,    81,    81,    82,    82,    83,    83,    84,    84,
      85,    85,    86,    86,    87,    88,    89,    90,    90,    91,
      92,    93,    94,    94,    95,    95,    95,    96,    96,    97,
      98,    98,    99,    99,   100,   100,   100,   100,   100,   101,
     101,   101,   102,   102,   102,   102
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     9,     1,     2,    12,     0,    13,     0,    37,
      35,    36,     6,    13,     6,    10,     0,     5,     0,     5,
       1,     1,     0,     2,     2,     2,     3,     1,     2,     6,
      16,     3,     1,     2,     5,     3,     6,     4,     9,     4,
       0,     3,     1,     2,    12,    10,    12,     8,    10,     1,
       1,     1,     0,     2,     2,     2
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (stkd, analysis, scanner, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, stkd, analysis, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, StackDescription *stkd, Analysis         *analysis, yyscan_t          scanner)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (stkd);
  YYUSE (analysis);
  YYUSE (scanner);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, StackDescription *stkd, Analysis         *analysis, yyscan_t          scanner)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, stkd, analysis, scanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, StackDescription *stkd, Analysis         *analysis, yyscan_t          scanner)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , stkd, analysis, scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, stkd, analysis, scanner); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, StackDescription *stkd, Analysis         *analysis, yyscan_t          scanner)
{
  YYUSE (yyvaluep);
  YYUSE (stkd);
  YYUSE (analysis);
  YYUSE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yytype)
    {
          case 67: /* "identifier"  */
#line 185 "stack_description_parser.y" /* yacc.c:1257  */
      { FREE_POINTER (free,                     ((*yyvaluep).string)) ; }
#line 1291 "stack_description_parser.c" /* yacc.c:1257  */
        break;

    case 68: /* "path to file"  */
#line 185 "stack_description_parser.y" /* yacc.c:1257  */
      { FREE_POINTER (free,                     ((*yyvaluep).string)) ; }
#line 1297 "stack_description_parser.c" /* yacc.c:1257  */
        break;

    case 86: /* layers_list  */
#line 186 "stack_description_parser.y" /* yacc.c:1257  */
      { FREE_POINTER (free_layers_list,         ((*yyvaluep).layer_p)) ; }
#line 1303 "stack_description_parser.c" /* yacc.c:1257  */
        break;

    case 87: /* layer  */
#line 186 "stack_description_parser.y" /* yacc.c:1257  */
      { FREE_POINTER (free_layers_list,         ((*yyvaluep).layer_p)) ; }
#line 1309 "stack_description_parser.c" /* yacc.c:1257  */
        break;

    case 88: /* source_layer  */
#line 186 "stack_description_parser.y" /* yacc.c:1257  */
      { FREE_POINTER (free_layers_list,         ((*yyvaluep).layer_p)) ; }
#line 1315 "stack_description_parser.c" /* yacc.c:1257  */
        break;

    case 89: /* layer_content  */
#line 186 "stack_description_parser.y" /* yacc.c:1257  */
      { FREE_POINTER (free_layers_list,         ((*yyvaluep).layer_p)) ; }
#line 1321 "stack_description_parser.c" /* yacc.c:1257  */
        break;


      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (StackDescription *stkd, Analysis         *analysis, yyscan_t          scanner)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

/* User initialization code.  */
#line 201 "stack_description_parser.y" /* yacc.c:1429  */
{
    first_wall_length = 0.0 ;
    last_wall_length  = 0.0 ;
    wall_length       = 0.0 ;
    channel_length    = 0.0 ;
    num_channels      = 0u ;
    num_dies          = 0u ;
}

#line 1420 "stack_description_parser.c" /* yacc.c:1429  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, scanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 3:
#line 238 "stack_description_parser.y" /* yacc.c:1646  */
    {
        stkd->MaterialsList = (yyvsp[0].material_p) ;

        (yyval.material_p) = (yyvsp[0].material_p) ;           // $1 will be the new last element in the list
    }
#line 1606 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 4:
#line 245 "stack_description_parser.y" /* yacc.c:1646  */
    {

        if (find_material_in_list (stkd->MaterialsList, (yyvsp[0].material_p)->Id) != NULL)
        {
            sprintf (error_message, "Material %s already declared", (yyvsp[0].material_p)->Id) ;

            FREE_POINTER (free_material, (yyvsp[0].material_p)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        (yyvsp[-1].material_p)->Next = (yyvsp[0].material_p) ;     // insert $2 at the end of the list
        (yyval.material_p) = (yyvsp[0].material_p) ;           // $2 will be the new last element in the list
    }
#line 1627 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 5:
#line 268 "stack_description_parser.y" /* yacc.c:1646  */
    {
        Material *material = (yyval.material_p) = alloc_and_init_material () ;

        if (material == NULL)
        {
            FREE_POINTER (free, (yyvsp[-10].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc material failed") ;

            YYABORT ;
        }

        material->Id                     = (yyvsp[-10].string) ;
        material->ThermalConductivity    = (SolidTC_t) (yyvsp[-6].double_v) ;
        material->VolumetricHeatCapacity = (SolidVHC_t) (yyvsp[-1].double_v) ;
    }
#line 1648 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 7:
#line 297 "stack_description_parser.y" /* yacc.c:1646  */
    {
        stkd->ConventionalHeatSink = alloc_and_init_conventional_heat_sink () ;

        if (stkd->ConventionalHeatSink == NULL)
        {
            stack_description_error (stkd, analysis, scanner, "Malloc conventional heat sink failed") ;

            YYABORT ;
        }

        stkd->ConventionalHeatSink->AmbientHTC         = (AmbientHTC_t) (yyvsp[-5].double_v) ;
        stkd->ConventionalHeatSink->AmbientTemperature = (yyvsp[-1].double_v) ;
    }
#line 1666 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 9:
#line 331 "stack_description_parser.y" /* yacc.c:1646  */
    {
        stkd->Channel = alloc_and_init_channel () ;

        if (stkd->Channel == NULL)
        {
            FREE_POINTER (free, (yyvsp[-18].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc channel failed") ;

            YYABORT ;
        }

        channel_length    = (yyvsp[-28].double_v) ;
        wall_length       = (yyvsp[-24].double_v) ;
        first_wall_length = ((yyvsp[-22].double_v) != 0.0) ? (yyvsp[-22].double_v) : (yyvsp[-24].double_v) ;
        last_wall_length  = ((yyvsp[-21].double_v) != 0.0) ? (yyvsp[-21].double_v) : (yyvsp[-24].double_v) ;

        stkd->Channel->ChannelModel      = TDICE_CHANNEL_MODEL_MC_4RM ;
        stkd->Channel->NLayers           = NUM_LAYERS_CHANNEL_4RM ;
        stkd->Channel->SourceLayerOffset = SOURCE_OFFSET_CHANNEL_4RM ;
        stkd->Channel->Height            = (yyvsp[-32].double_v) ;
        stkd->Channel->Coolant.FlowRate  = FLOW_RATE_FROM_MLMIN_TO_UM3SEC ((yyvsp[-13].double_v)) ;
        stkd->Channel->Coolant.HTCSide   = (yyvsp[-11].coolant_v).HTCSide ;
        stkd->Channel->Coolant.HTCTop    = (yyvsp[-11].coolant_v).HTCTop ;
        stkd->Channel->Coolant.HTCBottom = (yyvsp[-11].coolant_v).HTCBottom ;
        stkd->Channel->Coolant.VHC       = (yyvsp[-6].double_v) ;
        stkd->Channel->Coolant.TIn       = (yyvsp[-1].double_v) ;
        stkd->Channel->WallMaterial      = find_material_in_list (stkd->MaterialsList, (yyvsp[-18].string)) ;

        if (stkd->Channel->WallMaterial == NULL)
        {
            sprintf (error_message, "Unknown material %s", (yyvsp[-18].string)) ;

            FREE_POINTER (free, (yyvsp[-18].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        stkd->Channel->WallMaterial->Used++ ;

        FREE_POINTER (free, (yyvsp[-18].string)) ;
    }
#line 1715 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 10:
#line 385 "stack_description_parser.y" /* yacc.c:1646  */
    {
        stkd->Channel = alloc_and_init_channel () ;

        if (stkd->Channel == NULL)
        {
            FREE_POINTER (free, (yyvsp[-18].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc channel failed") ;

            YYABORT ;
        }

        stkd->Channel->ChannelModel      = TDICE_CHANNEL_MODEL_MC_2RM ;
        stkd->Channel->NLayers           = NUM_LAYERS_CHANNEL_2RM ;
        stkd->Channel->SourceLayerOffset = SOURCE_OFFSET_CHANNEL_2RM ;
        stkd->Channel->Height            = (yyvsp[-30].double_v) ;
        stkd->Channel->Length            = (yyvsp[-26].double_v) ;
        stkd->Channel->Pitch             = (yyvsp[-22].double_v) + (yyvsp[-26].double_v) ;
        stkd->Channel->Porosity          = stkd->Channel->Length / stkd->Channel->Pitch ;
        stkd->Channel->Coolant.FlowRate  = FLOW_RATE_FROM_MLMIN_TO_UM3SEC ((yyvsp[-13].double_v)) ;
        stkd->Channel->Coolant.HTCSide   = (yyvsp[-11].coolant_v).HTCSide ;
        stkd->Channel->Coolant.HTCTop    = (yyvsp[-11].coolant_v).HTCTop ;
        stkd->Channel->Coolant.HTCBottom = (yyvsp[-11].coolant_v).HTCBottom ;
        stkd->Channel->Coolant.VHC       = (yyvsp[-6].double_v) ;
        stkd->Channel->Coolant.TIn       = (yyvsp[-1].double_v) ;
        stkd->Channel->WallMaterial      = find_material_in_list (stkd->MaterialsList, (yyvsp[-18].string)) ;

        if (stkd->Channel->WallMaterial == NULL)
        {
            sprintf (error_message, "Unknown material %s", (yyvsp[-18].string)) ;

            FREE_POINTER (free, (yyvsp[-18].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        stkd->Channel->WallMaterial->Used++ ;

        FREE_POINTER (free, (yyvsp[-18].string)) ;
    }
#line 1762 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 11:
#line 437 "stack_description_parser.y" /* yacc.c:1646  */
    {
        stkd->Channel = alloc_and_init_channel () ;

        if (stkd->Channel == NULL)
        {
            FREE_POINTER (free, (yyvsp[-16].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc channel failed") ;

            YYABORT ;
        }

        stkd->Channel->Height                = (yyvsp[-32].double_v) ;
        stkd->Channel->Porosity              = POROSITY ((yyvsp[-28].double_v), (yyvsp[-24].double_v)) ;
        stkd->Channel->Pitch                 = (yyvsp[-24].double_v) ;
        stkd->Channel->ChannelModel          = (yyvsp[-20].channel_model_v) ;
        stkd->Channel->NLayers               = NUM_LAYERS_CHANNEL_2RM ;
        stkd->Channel->SourceLayerOffset     = SOURCE_OFFSET_CHANNEL_2RM ;
        stkd->Channel->Coolant.DarcyVelocity = (yyvsp[-12].double_v) ;
        stkd->Channel->Coolant.HTCSide       = 0.0 ;
        stkd->Channel->Coolant.HTCTop        = 0.0 ;
        stkd->Channel->Coolant.HTCBottom     = 0.0 ;
        stkd->Channel->Coolant.VHC           = (yyvsp[-6].double_v) ;
        stkd->Channel->Coolant.TIn           = (yyvsp[-1].double_v) ;
        stkd->Channel->WallMaterial          = find_material_in_list (stkd->MaterialsList, (yyvsp[-16].string)) ;

        if (stkd->Channel->WallMaterial == NULL)
        {
            sprintf (error_message, "Unknown material %s", (yyvsp[-16].string)) ;

            FREE_POINTER (free, (yyvsp[-16].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        stkd->Channel->WallMaterial->Used++ ;

        FREE_POINTER (free, (yyvsp[-16].string)) ;
    }
#line 1808 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 12:
#line 483 "stack_description_parser.y" /* yacc.c:1646  */
    {
        (yyval.coolant_v).HTCSide   = (yyvsp[-1].double_v) ;
        (yyval.coolant_v).HTCTop    = (yyvsp[-1].double_v) ;
        (yyval.coolant_v).HTCBottom = (yyvsp[-1].double_v) ;
    }
#line 1818 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 13:
#line 491 "stack_description_parser.y" /* yacc.c:1646  */
    {
        (yyval.coolant_v).HTCSide   = (yyvsp[-7].double_v) ;
        (yyval.coolant_v).HTCTop    = (yyvsp[-4].double_v) ;
        (yyval.coolant_v).HTCBottom = (yyvsp[-1].double_v) ;
    }
#line 1828 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 14:
#line 501 "stack_description_parser.y" /* yacc.c:1646  */
    {
        (yyval.coolant_v).HTCSide   = 0.0 ;
        (yyval.coolant_v).HTCTop    = (yyvsp[-1].double_v) ;
        (yyval.coolant_v).HTCBottom = (yyvsp[-1].double_v) ;
    }
#line 1838 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 15:
#line 508 "stack_description_parser.y" /* yacc.c:1646  */
    {
        (yyval.coolant_v).HTCSide   = 0.0 ;
        (yyval.coolant_v).HTCTop    = (yyvsp[-4].double_v) ;
        (yyval.coolant_v).HTCBottom = (yyvsp[-1].double_v) ;
    }
#line 1848 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 16:
#line 516 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.double_v) = 0.0 ; }
#line 1854 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 17:
#line 517 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.double_v) = (yyvsp[-1].double_v) ;  }
#line 1860 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 18:
#line 521 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.double_v) = 0.0 ; }
#line 1866 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 19:
#line 522 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.double_v) = (yyvsp[-1].double_v) ;  }
#line 1872 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 20:
#line 526 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.channel_model_v) = TDICE_CHANNEL_MODEL_PF_INLINE ;    }
#line 1878 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 21:
#line 527 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.channel_model_v) = TDICE_CHANNEL_MODEL_PF_STAGGERED ; }
#line 1884 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 22:
#line 537 "stack_description_parser.y" /* yacc.c:1646  */
    {
        (yyval.layer_p) = NULL ;    // The first layer in the list will be null
    }
#line 1892 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 23:
#line 542 "stack_description_parser.y" /* yacc.c:1646  */
    {
        if ((yyvsp[-1].layer_p) != NULL)
            JOIN_ELEMENTS ((yyvsp[0].layer_p), (yyvsp[-1].layer_p)) ; // this reverse the order !
        (yyval.layer_p) = (yyvsp[0].layer_p) ;                    // $2 will be the new reference to the list
    }
#line 1902 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 24:
#line 549 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.layer_p) = (yyvsp[0].layer_p) ; }
#line 1908 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 25:
#line 551 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.layer_p) = (yyvsp[0].layer_p) ; }
#line 1914 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 26:
#line 557 "stack_description_parser.y" /* yacc.c:1646  */
    {
        Layer *layer = (yyval.layer_p) = alloc_and_init_layer () ;

        if (layer == NULL)
        {
            FREE_POINTER (free, (yyvsp[-1].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc layer failed") ;

            YYABORT ;
        }

        layer->Height   = (yyvsp[-2].double_v) ;
        layer->Material = find_material_in_list (stkd->MaterialsList, (yyvsp[-1].string)) ;

        if (layer->Material == NULL)
        {
            sprintf (error_message, "Unknown material %s", (yyvsp[-1].string)) ;

            FREE_POINTER (free,       (yyvsp[-1].string)) ;
            FREE_POINTER (free_layer, layer) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        layer->Material->Used++ ;

        FREE_POINTER (free, (yyvsp[-1].string)) ;
    }
#line 1950 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 27:
#line 597 "stack_description_parser.y" /* yacc.c:1646  */
    {
        stkd->DiesList = (yyvsp[0].die_p) ;
        (yyval.die_p) = (yyvsp[0].die_p) ;         // $1 will be the new die in the list
    }
#line 1959 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 28:
#line 603 "stack_description_parser.y" /* yacc.c:1646  */
    {

        if (find_die_in_list (stkd->DiesList, (yyvsp[0].die_p)->Id) != NULL)
        {
            sprintf (error_message, "Die %s already declared", (yyvsp[0].die_p)->Id) ;

            FREE_POINTER (free_die, (yyvsp[0].die_p)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        (yyvsp[-1].die_p)->Next = (yyvsp[0].die_p) ;   // insert $2 at the end of the list
        (yyval.die_p) = (yyvsp[0].die_p) ;         // $2 will be the new last element in the list
    }
#line 1980 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 29:
#line 627 "stack_description_parser.y" /* yacc.c:1646  */
    {
        Die *die = (yyval.die_p) = alloc_and_init_die () ;

        if (die == NULL)
        {
            FREE_POINTER (free,             (yyvsp[-4].string)) ;
            FREE_POINTER (free_layers_list, (yyvsp[-2].layer_p)) ;
            FREE_POINTER (free_layer,       (yyvsp[-1].layer_p)) ;
            FREE_POINTER (free_layers_list, (yyvsp[0].layer_p));

            stack_description_error (stkd, analysis, scanner, "Malloc die failed") ;

            YYABORT ;
        }

        die->Id = (yyvsp[-4].string) ;
        die->SourceLayer = (yyvsp[-1].layer_p) ;

        // The layers within a die are declared in the stack file from top
        // to bottom but here we revert the order: the first layer in the list
        // LayersList will be the bottom-most layer in the die

        if ((yyvsp[0].layer_p) != NULL)
        {
            // if there are layers below the source,
            // then the list of layers begins with $6

            die->BottomLayer = (yyvsp[0].layer_p) ;

            die->NLayers++ ;
            die->SourceLayerOffset++ ;

            // $6 moved until the end ..

            while ((yyvsp[0].layer_p)->Next != NULL)
            {
                (yyvsp[0].layer_p) = (yyvsp[0].layer_p)->Next ;

                die->NLayers++ ;
                die->SourceLayerOffset++ ;
            }

            // the list $6 continues with the source layer $5

            JOIN_ELEMENTS ((yyvsp[0].layer_p), (yyvsp[-1].layer_p)) ;
        }
        else
        {
            // if there aren't layers below the source, the list of layers
            // begins directly with the source layer $5

            die->BottomLayer = (yyvsp[-1].layer_p) ;
        }

        die->NLayers++ ;

        if ((yyvsp[-2].layer_p) != NULL)
        {
            // if there are layers above the source
            // $5 is connected to the list $4

            JOIN_ELEMENTS ((yyvsp[-1].layer_p), (yyvsp[-2].layer_p)) ;

            die->NLayers++ ;

            // $4 moved until the end ..

            while ((yyvsp[-2].layer_p)->Next != NULL)
            {
                (yyvsp[-2].layer_p) = (yyvsp[-2].layer_p)->Next ;

                die->NLayers++ ;
            }

            // the list finishes with the last layer in $4

            die->TopLayer = (yyvsp[-2].layer_p) ;
        }
        else
        {
            // if there aren't layers below the source,
            // The list finishes with the source layer $5

            die->TopLayer = (yyvsp[-1].layer_p) ;
        }
    }
#line 2071 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 30:
#line 724 "stack_description_parser.y" /* yacc.c:1646  */
    {
        stkd->Dimensions = alloc_and_init_dimensions () ;

        if (stkd->Dimensions == NULL)
        {
            stack_description_error (stkd, analysis, scanner, "Malloc dimensions failed") ;
            YYABORT ;
        }

        stkd->Dimensions->Chip.Length = (yyvsp[-11].double_v) ;
        stkd->Dimensions->Chip.Width  = (yyvsp[-8].double_v) ;

        stkd->Dimensions->Cell.ChannelLength   = (yyvsp[-4].double_v) ;
        stkd->Dimensions->Cell.FirstWallLength = (yyvsp[-4].double_v) ;
        stkd->Dimensions->Cell.LastWallLength  = (yyvsp[-4].double_v) ;
        stkd->Dimensions->Cell.WallLength      = (yyvsp[-4].double_v) ;

        stkd->Dimensions->Cell.Width  = (yyvsp[-1].double_v) ;

        stkd->Dimensions->Grid.NRows    = ((yyvsp[-8].double_v) / (yyvsp[-1].double_v)) ;
        stkd->Dimensions->Grid.NColumns = ((yyvsp[-11].double_v) / (yyvsp[-4].double_v)) ;


        if (stkd->Channel != NULL)
        {
            if (stkd->Channel->ChannelModel == TDICE_CHANNEL_MODEL_MC_4RM)
            {
                stkd->Dimensions->Cell.ChannelLength   = channel_length ;
                stkd->Dimensions->Cell.FirstWallLength = first_wall_length ;
                stkd->Dimensions->Cell.LastWallLength  = last_wall_length ;
                stkd->Dimensions->Cell.WallLength      = wall_length ;

                CellDimension_t ratio
                    = ((yyvsp[-11].double_v) - first_wall_length - last_wall_length -channel_length)
                    /
                    (channel_length + wall_length) ;

                if ( ratio - (int) ratio != 0)
                {
                    stack_description_error (stkd, analysis, scanner, "Error: cell dimensions does not fit the chip length correctly") ;

                    YYABORT ;
                }

                stkd->Dimensions->Grid.NColumns = 2 * ratio + 3 ;

                if ((stkd->Dimensions->Grid.NColumns & 1) == 0)
                {
                    stack_description_error (stkd, analysis, scanner, "Error: colum number must be odd when channel is declared") ;

                    YYABORT ;
                }

                // Check the number of columns

                if (stkd->Dimensions->Grid.NColumns < 3)
                {
                    stack_description_error (stkd, analysis, scanner, "Error: not enough columns") ;

                    YYABORT ;
                }

                stkd->Channel->NChannels = ((stkd->Dimensions->Grid.NColumns - 1 )  / 2) ;
            }
            else if (stkd->Channel->ChannelModel == TDICE_CHANNEL_MODEL_MC_2RM)
            {
                stkd->Channel->NChannels = (((yyvsp[-11].double_v) / stkd->Channel->Pitch) + 0.5) ; // round function
            }
        }
    }
#line 2146 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 31:
#line 804 "stack_description_parser.y" /* yacc.c:1646  */
    {
        if (num_dies == 0u)
        {
            stack_description_error (stkd, analysis, scanner, "Error: stack must contain at least one die") ;

            YYABORT ;
        }

        if (stkd->BottomStackElement->Type == TDICE_STACK_ELEMENT_CHANNEL)
        {
            stack_description_error (stkd, analysis, scanner, "Error: cannot declare a channel as bottom-most stack element") ;

            YYABORT ;
        }

        if (stkd->ConventionalHeatSink == NULL && stkd->Channel == NULL)

            fprintf (stderr, "Warning: neither heat sink nor channel has been declared\n") ;


        FOR_EVERY_ELEMENT_IN_LIST_NEXT (Material, material, stkd->MaterialsList)

            if (material->Used == 0)

                fprintf (stderr, "Warning: material %s declared but not used\n", material->Id) ;


        FOR_EVERY_ELEMENT_IN_LIST_NEXT (Die, die, stkd->DiesList)

            if (die->Used == 0)

                fprintf (stderr, "Warning: die %s declared but not used\n", die->Id) ;

        if (stkd->ConventionalHeatSink != NULL)
        {
            if (stkd->TopStackElement->Type == TDICE_STACK_ELEMENT_LAYER)
            {
                stkd->ConventionalHeatSink->TopLayer = stkd->TopStackElement->Pointer.Layer ;
            }
            else
            {
                stkd->ConventionalHeatSink->TopLayer = stkd->TopStackElement->Pointer.Die->TopLayer ;
            }
        }

        // Counts the number of layers and fix the layer offset starting from
        // the bottom most element in the stack. This operation can be done only
        // here since the parser processes elements in the stack from the top most.

        CellIndex_t layer_index = 0u ;

        FOR_EVERY_ELEMENT_IN_LIST_NEXT (StackElement, stk_el, stkd->BottomStackElement)
        {
            stk_el->Offset = layer_index ;
            layer_index   += stk_el->NLayers ;
        }

        stkd->Dimensions->Grid.NLayers = layer_index ;

        // Evaluate the number of cells and nonzero elements

        stkd->Dimensions->Grid.NCells
            =   get_number_of_layers (stkd->Dimensions)
                * get_number_of_rows (stkd->Dimensions)
                * get_number_of_columns (stkd->Dimensions) ;

        if (stkd->Dimensions->Grid.NCells >  INT32_MAX)
        {
            sprintf (error_message, "%d are too many cells ... (SuperLU uses 'int')", stkd->Dimensions->Grid.NCells) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        if (stkd->Channel == NULL)

            compute_number_of_connections

                (stkd->Dimensions, num_channels, TDICE_CHANNEL_MODEL_NONE) ;

        else

            compute_number_of_connections

                (stkd->Dimensions, num_channels, stkd->Channel->ChannelModel) ;

    }
#line 2239 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 32:
#line 897 "stack_description_parser.y" /* yacc.c:1646  */
    {
        if (   stkd->TopStackElement == NULL && (yyvsp[0].stack_element_p)->Type == TDICE_STACK_ELEMENT_CHANNEL)
        {
            stack_description_error (stkd, analysis, scanner, "Error: cannot declare a channel as top-most stack element") ;

            YYABORT ;
        }

        stkd->TopStackElement    = (yyvsp[0].stack_element_p) ;
        stkd->BottomStackElement = (yyvsp[0].stack_element_p) ;
        (yyval.stack_element_p) = (yyvsp[0].stack_element_p) ;
    }
#line 2256 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 33:
#line 911 "stack_description_parser.y" /* yacc.c:1646  */
    {
        if (find_stack_element_in_list ((yyvsp[-1].stack_element_p), (yyvsp[0].stack_element_p)->Id) != NULL)
        {
            sprintf (error_message, "Stack element %s already declared", (yyvsp[0].stack_element_p)->Id) ;

            FREE_POINTER (free_stack_element, (yyvsp[0].stack_element_p)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        if (   (yyvsp[-1].stack_element_p)->Type == TDICE_STACK_ELEMENT_CHANNEL
            && (yyvsp[0].stack_element_p)->Type == TDICE_STACK_ELEMENT_CHANNEL)
        {
            stack_description_error (stkd, analysis, scanner, "Error: cannot declare two consecutive channels") ;

            YYABORT ;
        }

        JOIN_ELEMENTS ((yyvsp[0].stack_element_p), (yyvsp[-1].stack_element_p)) ;

        stkd->BottomStackElement = (yyvsp[0].stack_element_p) ;
        (yyval.stack_element_p) = (yyvsp[0].stack_element_p) ;                 // $2 will be the last stack element in the list
    }
#line 2286 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 34:
#line 943 "stack_description_parser.y" /* yacc.c:1646  */
    {
        StackElement *stack_element = (yyval.stack_element_p) = alloc_and_init_stack_element () ;

        if (stack_element == NULL)
        {
            FREE_POINTER (free, (yyvsp[-3].string)) ;
            FREE_POINTER (free, (yyvsp[-1].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc stack element failed") ;

            YYABORT ;
        }

        Layer *layer = alloc_and_init_layer () ;

        if (layer == NULL)
        {
            FREE_POINTER (free,               (yyvsp[-3].string)) ;
            FREE_POINTER (free,               (yyvsp[-1].string)) ;
            FREE_POINTER (free_stack_element, stack_element) ;

            stack_description_error (stkd, analysis, scanner, "Malloc layer failed") ;

            YYABORT ;
        }

        layer->Height   = (yyvsp[-2].double_v) ;
        layer->Material = find_material_in_list (stkd->MaterialsList, (yyvsp[-1].string)) ;

        if (layer->Material == NULL)
        {
            sprintf (error_message, "Unknown material %s", (yyvsp[-1].string)) ;

            FREE_POINTER (free,               (yyvsp[-3].string)) ;
            FREE_POINTER (free,               (yyvsp[-1].string)) ;
            FREE_POINTER (free_stack_element, stack_element) ;
            FREE_POINTER (free_layer,         layer) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        layer->Material->Used++ ;

        FREE_POINTER (free, (yyvsp[-1].string)) ;

        stack_element->Type          = TDICE_STACK_ELEMENT_LAYER ;
        stack_element->Pointer.Layer = layer ;
        stack_element->Id            = (yyvsp[-3].string) ;
        stack_element->NLayers       = 1 ;
    }
#line 2343 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 35:
#line 997 "stack_description_parser.y" /* yacc.c:1646  */
    {
        num_channels++ ;

        if (stkd->Channel == NULL)
        {
            stack_description_error (stkd, analysis, scanner, "Error: channel used in stack but not declared") ;

            YYABORT ;
        }

        StackElement *stack_element = (yyval.stack_element_p) = alloc_and_init_stack_element () ;

        if (stack_element == NULL)
        {
            FREE_POINTER (free, (yyvsp[-1].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc stack element failed") ;

            YYABORT ;
        }

        stack_element->Type            = TDICE_STACK_ELEMENT_CHANNEL ;
        stack_element->Pointer.Channel = stkd->Channel ; // This might be NULL !!!
        stack_element->Id              = (yyvsp[-1].string) ;
        stack_element->NLayers         = stkd->Channel->NLayers ;
    }
#line 2374 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 36:
#line 1027 "stack_description_parser.y" /* yacc.c:1646  */
    {
        num_dies++ ;

        StackElement *stack_element = (yyval.stack_element_p) = alloc_and_init_stack_element () ;

        if (stack_element == NULL)
        {
            FREE_POINTER (free, (yyvsp[-4].string)) ;
            FREE_POINTER (free, (yyvsp[-3].string)) ;
            FREE_POINTER (free, (yyvsp[-1].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc stack element failed") ;

            YYABORT ;
        }

        stack_element->Type        = TDICE_STACK_ELEMENT_DIE ;
        stack_element->Id          = (yyvsp[-4].string) ;
        stack_element->Pointer.Die = find_die_in_list (stkd->DiesList, (yyvsp[-3].string)) ;

        if (stack_element->Pointer.Die == NULL)
        {
            sprintf (error_message, "Unknown die %s", (yyvsp[-3].string)) ;

            FREE_POINTER (free,               (yyvsp[-3].string)) ;
            FREE_POINTER (free,               (yyvsp[-1].string)) ;
            FREE_POINTER (free_stack_element, stack_element) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            YYABORT ;
        }

        stack_element->Pointer.Die->Used++ ;

        stack_element->NLayers = stack_element->Pointer.Die->NLayers ;
        stack_element->Floorplan = alloc_and_init_floorplan () ;

        if (stack_element->Floorplan == NULL)
        {
            FREE_POINTER (free,               (yyvsp[-3].string)) ;
            FREE_POINTER (free,               (yyvsp[-1].string)) ;
            FREE_POINTER (free_stack_element, stack_element) ;

            stack_description_error (stkd, analysis, scanner, "Malloc floorplan failed") ;

            YYABORT ;
        }

        if (   fill_floorplan (stack_element->Floorplan, stkd->Dimensions, (yyvsp[-1].string))
            == TDICE_FAILURE)
        {
            FREE_POINTER (free,                   (yyvsp[-3].string)) ;
            FREE_POINTER (free,                   (yyvsp[-1].string)) ;
            FREE_POINTER (free_stack_description, stkd) ;

            YYABORT ; // CHECKME error messages printed in this case ....
        }

        FREE_POINTER (free, (yyvsp[-1].string)) ;  // FIXME check memory leak
        FREE_POINTER (free, (yyvsp[-3].string)) ;
    }
#line 2441 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 37:
#line 1098 "stack_description_parser.y" /* yacc.c:1646  */
    {
        // StepTime is set to 1 to avoid division by zero when computing
        // the capacitance of a thermal cell

        analysis->AnalysisType = TDICE_ANALYSIS_TYPE_STEADY ;
        analysis->StepTime     = (Time_t) 1.0 ;
        analysis->SlotTime     = (Time_t) 0.0 ;
        analysis->SlotLength   = 1u ; // CHECKME
    }
#line 2455 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 38:
#line 1110 "stack_description_parser.y" /* yacc.c:1646  */
    {
        if ((yyvsp[-1].double_v) < (yyvsp[-4].double_v))
        {
            stack_description_error

                (stkd, analysis, scanner, "Slot time must be higher than StepTime") ;

            YYABORT ;
        }

        if ((yyvsp[-4].double_v) <= 0.0)
        {
            stack_description_error

                (stkd, analysis, scanner, "StepTime must be a positive value") ;

            YYABORT ;
        }

        if ((yyvsp[-1].double_v) <= 0.0)
        {
            stack_description_error

                (stkd, analysis, scanner, "SlotTime must be a positive value") ;

            YYABORT ;
        }

        analysis->AnalysisType = TDICE_ANALYSIS_TYPE_TRANSIENT ;
        analysis->StepTime     = (Time_t) (yyvsp[-4].double_v) ;
        analysis->SlotTime     = (Time_t) (yyvsp[-1].double_v) ;
        analysis->SlotLength   = (Quantity_t) ( (yyvsp[-1].double_v) / (yyvsp[-4].double_v) ) ;
    }
#line 2493 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 39:
#line 1152 "stack_description_parser.y" /* yacc.c:1646  */
    {
        analysis->InitialTemperature = (Temperature_t) (yyvsp[-1].double_v);
    }
#line 2501 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 42:
#line 1172 "stack_description_parser.y" /* yacc.c:1646  */
    {
        add_inspection_point_to_analysis (analysis, (yyvsp[0].inspection_point_p)) ;
     }
#line 2509 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 43:
#line 1179 "stack_description_parser.y" /* yacc.c:1646  */
    {
        add_inspection_point_to_analysis (analysis, (yyvsp[0].inspection_point_p)) ;
     }
#line 2517 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 44:
#line 1193 "stack_description_parser.y" /* yacc.c:1646  */
    {
        StackElement *stack_element =

            find_stack_element_in_list (stkd->BottomStackElement, (yyvsp[-9].string)) ;

        if (stack_element == NULL)
        {
            sprintf (error_message, "Unknown stack element %s", (yyvsp[-9].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-9].string)) ;
            FREE_POINTER (free, (yyvsp[-3].string)) ;

            YYABORT ;
        }

        Tcell *tcell = alloc_and_init_tcell () ;

        if (tcell == NULL)
        {
            FREE_POINTER (free, (yyvsp[-9].string)) ;
            FREE_POINTER (free, (yyvsp[-3].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc tcell failed") ;

            YYABORT ;
        }

        align_tcell (tcell, (yyvsp[-7].double_v), (yyvsp[-5].double_v), stkd->Dimensions) ;

        InspectionPoint *inspection_point = (yyval.inspection_point_p) = alloc_and_init_inspection_point () ;

        if (inspection_point == NULL)
        {
            FREE_POINTER (free, (yyvsp[-9].string)) ;
            FREE_POINTER (free, (yyvsp[-3].string)) ;

            FREE_POINTER (free_tcell, tcell) ;

            stack_description_error (stkd, analysis, scanner, "Malloc inspection point command failed") ;

            YYABORT ;
        }

        inspection_point->Type          = TDICE_OUTPUT_TYPE_TCELL ;
        inspection_point->Instant       = (yyvsp[-2].output_instant_v) ;
        inspection_point->FileName      = (yyvsp[-3].string) ;
        inspection_point->Pointer.Tcell = tcell ;
        inspection_point->StackElement  = stack_element ;

        FREE_POINTER (free, (yyvsp[-9].string)) ;
     }
#line 2575 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 45:
#line 1254 "stack_description_parser.y" /* yacc.c:1646  */
    {
        StackElement *stack_element =

            find_stack_element_in_list (stkd->BottomStackElement, (yyvsp[-7].string)) ;

        if (stack_element == NULL)
        {
            sprintf (error_message, "Unknown stack element %s", (yyvsp[-7].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            YYABORT ;
        }

        if (stack_element->Type != TDICE_STACK_ELEMENT_DIE)
        {
            sprintf (error_message, "The stack element %s must be a die", (yyvsp[-7].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            YYABORT ;
        }

        Tflp *tflp = alloc_and_init_tflp () ;

        if (tflp == NULL)
        {
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc tflp failed") ;

            YYABORT ;
        }

        tflp->Quantity = (yyvsp[-3].output_quantity_v) ;

        InspectionPoint *inspection_point = (yyval.inspection_point_p) = alloc_and_init_inspection_point () ;

        if (inspection_point == NULL)
        {
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            FREE_POINTER (free_tflp, tflp) ;

            stack_description_error (stkd, analysis, scanner, "Malloc inspection point command failed") ;

            YYABORT ;
        }

        inspection_point->Type         = TDICE_OUTPUT_TYPE_TFLP ;
        inspection_point->Instant      = (yyvsp[-2].output_instant_v) ;
        inspection_point->FileName     = (yyvsp[-5].string) ;
        inspection_point->Pointer.Tflp = tflp ;
        inspection_point->StackElement = stack_element ;

        FREE_POINTER (free, (yyvsp[-7].string)) ;
     }
#line 2645 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 46:
#line 1328 "stack_description_parser.y" /* yacc.c:1646  */
    {
        StackElement *stack_element =

            find_stack_element_in_list (stkd->BottomStackElement, (yyvsp[-9].string)) ;

        if (stack_element == NULL)
        {
            sprintf (error_message, "Unknown stack element %s", (yyvsp[-9].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-9].string)) ;
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            YYABORT ;
        }

        if (stack_element->Type != TDICE_STACK_ELEMENT_DIE)
        {
            sprintf (error_message, "The stack element %s must be a die", (yyvsp[-9].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-9].string)) ;
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            YYABORT ;
        }

        FloorplanElement *floorplan_element = find_floorplan_element_in_list

            (stack_element->Floorplan->ElementsList, (yyvsp[-7].string)) ;

        if (floorplan_element == NULL)
        {
            sprintf (error_message, "Unknown floorplan element %s", (yyvsp[-7].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-9].string)) ;
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            YYABORT ;
        }

        Tflpel *tflpel = alloc_and_init_tflpel () ;

        if (tflpel == NULL)
        {
            FREE_POINTER (free, (yyvsp[-9].string)) ;
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc Tflpel failed") ;

            YYABORT ;
        }

        tflpel->FloorplanElement = floorplan_element ;
        tflpel->Quantity         = (yyvsp[-3].output_quantity_v) ;

        InspectionPoint *inspection_point = (yyval.inspection_point_p) = alloc_and_init_inspection_point () ;

        if (inspection_point == NULL)
        {
            FREE_POINTER (free, (yyvsp[-9].string)) ;
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            FREE_POINTER (free_tflpel, tflpel) ;

            stack_description_error (stkd, analysis, scanner, "Malloc inspection point command failed") ;

            YYABORT ;
        }

        inspection_point->Type           = TDICE_OUTPUT_TYPE_TFLPEL ;
        inspection_point->Instant        = (yyvsp[-2].output_instant_v) ;
        inspection_point->FileName       = (yyvsp[-5].string) ;
        inspection_point->Pointer.Tflpel = tflpel ;
        inspection_point->StackElement   = stack_element ;

        FREE_POINTER (free, (yyvsp[-9].string)) ;
        FREE_POINTER (free, (yyvsp[-7].string)) ;
     }
#line 2738 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 47:
#line 1423 "stack_description_parser.y" /* yacc.c:1646  */
    {
        StackElement *stack_element =

            find_stack_element_in_list (stkd->BottomStackElement, (yyvsp[-5].string)) ;

        if (stack_element == NULL)
        {
            sprintf (error_message, "Unknown stack element %s", (yyvsp[-5].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-5].string)) ;
            FREE_POINTER (free, (yyvsp[-3].string)) ;

            YYABORT ;
        }

        InspectionPoint *inspection_point = (yyval.inspection_point_p) = alloc_and_init_inspection_point () ;

        if (inspection_point == NULL)
        {
            FREE_POINTER (free, (yyvsp[-5].string)) ;
            FREE_POINTER (free, (yyvsp[-3].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc inspection point command failed") ;

            YYABORT ;
        }

        inspection_point->Type         = TDICE_OUTPUT_TYPE_TMAP ;
        inspection_point->Instant      = (yyvsp[-2].output_instant_v) ;
        inspection_point->FileName     = (yyvsp[-3].string) ;
        inspection_point->StackElement = stack_element ;

        FREE_POINTER (free, (yyvsp[-5].string)) ;
     }
#line 2779 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 48:
#line 1467 "stack_description_parser.y" /* yacc.c:1646  */
    {
        StackElement *stack_element =

            find_stack_element_in_list (stkd->BottomStackElement, (yyvsp[-7].string)) ;

        if (stack_element == NULL)
        {
            sprintf (error_message, "Unknown stack element %s", (yyvsp[-7].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            YYABORT ;
        }

        if (stack_element->Type != TDICE_STACK_ELEMENT_CHANNEL)
        {
            sprintf (error_message, "The stack element %s must be a channel", (yyvsp[-7].string)) ;

            stack_description_error (stkd, analysis, scanner, error_message) ;

            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            YYABORT ;
        }

        InspectionPoint *inspection_point = (yyval.inspection_point_p) = alloc_and_init_inspection_point () ;

        if (inspection_point == NULL)
        {
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc inspection point command failed") ;

            YYABORT ;
        }

        Tcoolant *tcoolant = alloc_and_init_tcoolant () ;

        if (tcoolant == NULL)
        {
            FREE_POINTER (free, (yyvsp[-7].string)) ;
            FREE_POINTER (free, (yyvsp[-5].string)) ;

            stack_description_error (stkd, analysis, scanner, "Malloc tcoolant failed") ;

            YYABORT ;
        }

        tcoolant->Quantity = (yyvsp[-3].output_quantity_v) ;

        inspection_point->Type             = TDICE_OUTPUT_TYPE_TCOOLANT ;
        inspection_point->Instant          = (yyvsp[-2].output_instant_v) ;
        inspection_point->FileName         = (yyvsp[-5].string) ;
        inspection_point->Pointer.Tcoolant = tcoolant ;
        inspection_point->StackElement     = stack_element ;

        FREE_POINTER (free, (yyvsp[-7].string)) ;
     }
#line 2847 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 49:
#line 1535 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.output_quantity_v) =  TDICE_OUTPUT_QUANTITY_MAXIMUM ; }
#line 2853 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 50:
#line 1536 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.output_quantity_v) =  TDICE_OUTPUT_QUANTITY_MINIMUM ; }
#line 2859 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 51:
#line 1537 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.output_quantity_v) =  TDICE_OUTPUT_QUANTITY_AVERAGE ; }
#line 2865 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 52:
#line 1543 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.output_instant_v) =  TDICE_OUTPUT_INSTANT_FINAL ; }
#line 2871 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 53:
#line 1544 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.output_instant_v) =  TDICE_OUTPUT_INSTANT_STEP ;  }
#line 2877 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 54:
#line 1545 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.output_instant_v) =  TDICE_OUTPUT_INSTANT_SLOT ;  }
#line 2883 "stack_description_parser.c" /* yacc.c:1646  */
    break;

  case 55:
#line 1546 "stack_description_parser.y" /* yacc.c:1646  */
    { (yyval.output_instant_v) =  TDICE_OUTPUT_INSTANT_FINAL ; }
#line 2889 "stack_description_parser.c" /* yacc.c:1646  */
    break;


#line 2893 "stack_description_parser.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (stkd, analysis, scanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (stkd, analysis, scanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, stkd, analysis, scanner);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, stkd, analysis, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (stkd, analysis, scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, stkd, analysis, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, stkd, analysis, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 1549 "stack_description_parser.y" /* yacc.c:1906  */


void stack_description_error
(
    StackDescription *stkd,
    Analysis          __attribute__ ((unused)) *analysis,
    yyscan_t          scanner,
    String_t          message
)
{
    fprintf (stack_description_get_out (scanner),
             "%s:%d: %s\n",
            stkd->FileName, stack_description_get_lineno (scanner), message) ;

    FREE_POINTER (free_stack_description, stkd) ;
}
