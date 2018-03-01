pipeline = "outorder";

fastfwd = 0L;

qsim_server = "127.0.0.1";
qsim_port = "1234";

fetch_width = 4;
allocate_width = 6;
execute_width = 5;
commit_width = 4;

instQ_size = 64;
RS_size = 84;
LDQ_size = 48;
STQ_size = 32;
ROB_size = 242;

IL1:
{
	size = 0x8000; //32K
	line_width = 32;
	assoc = 4;
};

DL1:
{
	size = 0x8000; //32K
	line_width = 32;
	assoc = 4;
};

FU_INT: 
{
	delay = 1;
	issue_rate = 1;
	port = [0, 1, 2];
};

FU_MUL: 
{
	delay = 5;
 	issue_rate = 1;
	port = [0];
};

FU_FP: 
{
	delay = 3;
	issue_rate = 1;
	port = [1];
};

FU_MOV: 
{
	delay = 1;
	issue_rate = 1;
	port = [0, 1, 2];
};

FU_BR: 
{
	delay = 1;
	issue_rate = 1;
	port = [2];
};

FU_LD: 
{
	delay = 1;
	issue_rate = 1;
	port = [3];
};

FU_ST: 
{
	delay = 1;
	issue_rate = 1;
	port = [4];
};

