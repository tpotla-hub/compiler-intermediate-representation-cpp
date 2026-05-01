#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include "lexer.h"
#include "execute.h"

using namespace std;

// Lexical analyzer instance
LexicalAnalyzer scanner;
Token token;

// Variable name to memory location mapping
static map<string, int> varTable;

// -- Utility Functions --

string toLowercase(const string& s) {
    string lower = s;
    transform(lower.begin(), lower.end(), lower.begin(),
              [](unsigned char c) { return tolower(c); });
    return lower;
}

void adjustIfKeyword(Token &tk) {
    if (tk.token_type == ID) {
        string lex = toLowercase(tk.lexeme);
        if (lex == "switch") tk.token_type = SWITCH;
        else if (lex == "case") tk.token_type = CASE;
        else if (lex == "default") tk.token_type = DEFAULT;
        else if (lex == "var") tk.token_type = VAR;
        else if (lex == "for") tk.token_type = FOR;
        else if (lex == "if") tk.token_type = IF;
        else if (lex == "while") tk.token_type = WHILE;
        else if (lex == "input") tk.token_type = INPUT;
        else if (lex == "output") tk.token_type = OUTPUT;
        else if (lex == "array") tk.token_type = ARRAY;
    }
}

void fetchNextToken() {
    token = scanner.GetToken();
    adjustIfKeyword(token);
}

void expect(TokenType expected) {
    if (token.token_type == expected) {
        fetchNextToken();
    } else {
        cerr << "Error: Expected " << expected << ", got '" 
             << token.lexeme << "' on line " << token.line_no << endl;
        exit(1);
    }
}

// -- Instruction List Utilities --

InstructionNode* linkInstructions(InstructionNode* head, InstructionNode* tail) {
    if (!head) return tail;
    if (!tail) return head;
    head->next = linkInstructions(head->next, tail);
    return head;
}

InstructionNode* makeNoop() {
    InstructionNode* n = new InstructionNode;
    n->type = NOOP;
    n->next = nullptr;
    n->assign_inst.op = OPERATOR_NONE;
    n->assign_inst.lhs_loc = -1;
    n->assign_inst.op1_loc = -1;
    n->assign_inst.op2_loc = -1;
    n->input_inst.var_loc = -1;
    n->output_inst.var_loc = -1;
    n->cjmp_inst.op1_loc = -1;
    n->cjmp_inst.op2_loc = -1;
    n->cjmp_inst.condition_op = CONDITION_NOTEQUAL;
    n->cjmp_inst.target = nullptr;
    n->jmp_inst.target = nullptr;
    return n;
}

// -- Parsing Function Forward Declarations --
InstructionNode* parse_program();
InstructionNode* parse_var_declarations();
InstructionNode* parse_id_sequence();
InstructionNode* parse_code_block();
InstructionNode* parse_statement_list();
InstructionNode* parse_statement();
InstructionNode* parse_assign_statement();
InstructionNode* parse_input_statement();
InstructionNode* parse_output_statement();
InstructionNode* parse_if_statement();
InstructionNode* parse_while_statement();
InstructionNode* parse_switch_statement();
InstructionNode* parse_for_statement();
void parse_input_values();
int parse_value();
void parse_condition_expr(int &left, int &right, ConditionalOperatorType &op);

// -- Parser Implementations --

InstructionNode* parse_program() {
    parse_var_declarations();
    InstructionNode* body = parse_code_block();
    parse_input_values();
    return body;
}

InstructionNode* parse_var_declarations() {
    parse_id_sequence();
    expect(SEMICOLON);
    return nullptr;
}

InstructionNode* parse_id_sequence() {
    if (token.token_type != ID) {
        cerr << "Error: Variable expected in declaration." << endl;
        exit(1);
    }
    string name = token.lexeme;
    if (varTable.find(name) == varTable.end()) {
        varTable[name] = next_available;
        mem[next_available] = 0;
        next_available++;
    }
    expect(ID);
    if (token.token_type == COMMA) {
        expect(COMMA);
        parse_id_sequence();
    }
    return nullptr;
}

InstructionNode* parse_code_block() {
    expect(LBRACE);
    InstructionNode* list = parse_statement_list();
    expect(RBRACE);
    return list ? list : makeNoop();
}

InstructionNode* parse_statement_list() {
    if (token.token_type == RBRACE || token.token_type == END_OF_FILE)
        return nullptr;
    InstructionNode* stmt = parse_statement();
    InstructionNode* rest = parse_statement_list();
    return linkInstructions(stmt, rest);
}

InstructionNode* parse_statement() {
    if (token.token_type == ID) {
        string lex = toLowercase(token.lexeme);
        if (lex == "input")   token.token_type = INPUT;
        else if (lex == "output") token.token_type = OUTPUT;
        else if (lex == "if") token.token_type = IF;
        else if (lex == "while") token.token_type = WHILE;
        else if (lex == "switch") token.token_type = SWITCH;
        else if (lex == "for") token.token_type = FOR;
    }
    switch (token.token_type) {
        case ID:     return parse_assign_statement();
        case INPUT:  return parse_input_statement();
        case OUTPUT: return parse_output_statement();
        case IF:     return parse_if_statement();
        case WHILE:  return parse_while_statement();
        case SWITCH: return parse_switch_statement();
        case FOR:    return parse_for_statement();
        default:
            cerr << "Error: Unexpected token '" << token.lexeme
                 << "' at line " << token.line_no << endl;
            exit(1);
    }
    return nullptr;
}

InstructionNode* parse_assign_statement() {
    string lhs = token.lexeme;
    if (varTable.find(lhs) == varTable.end()) {
        cerr << "Error: Undefined variable '" << lhs << "' at line "
             << token.line_no << endl;
        exit(1);
    }
    int lhs_loc = varTable[lhs];
    expect(ID);
    expect(EQUAL);

    InstructionNode* assign = new InstructionNode;
    assign->type = ASSIGN;
    assign->next = nullptr;
    assign->assign_inst.lhs_loc = lhs_loc;
    assign->assign_inst.op = OPERATOR_NONE;
    assign->assign_inst.op1_loc = -1;
    assign->assign_inst.op2_loc = -1;

    int first_op_loc = parse_value();
    if (token.token_type == PLUS || token.token_type == MINUS ||
        token.token_type == MULT || token.token_type == DIV) {
        ArithmeticOperatorType op;
        if      (token.token_type == PLUS)  op = OPERATOR_PLUS;
        else if (token.token_type == MINUS) op = OPERATOR_MINUS;
        else if (token.token_type == MULT)  op = OPERATOR_MULT;
        else                                op = OPERATOR_DIV;
        expect(token.token_type);
        int second_op_loc = parse_value();
        assign->assign_inst.op = op;
        assign->assign_inst.op1_loc = first_op_loc;
        assign->assign_inst.op2_loc = second_op_loc;
    } else {
        assign->assign_inst.op1_loc = first_op_loc;
    }
    expect(SEMICOLON);
    return assign;
}

InstructionNode* parse_input_statement() {
    expect(INPUT);
    string name = token.lexeme;
    if (varTable.find(name) == varTable.end()) {
        cerr << "Error: Input variable '" << name << "' undeclared (line "
             << token.line_no << ")" << endl;
        exit(1);
    }
    int loc = varTable[name];
    expect(ID);
    expect(SEMICOLON);
    InstructionNode* node = new InstructionNode;
    node->type = IN;
    node->next = nullptr;
    node->input_inst.var_loc = loc;
    return node;
}

InstructionNode* parse_output_statement() {
    expect(OUTPUT);
    int loc;
    if (token.token_type == ID) {
        string name = token.lexeme;
        if (varTable.find(name) == varTable.end()) {
            cerr << "Error: Output variable '" << name << "' undeclared (line "
                 << token.line_no << ")" << endl;
            exit(1);
        }
        loc = varTable[name];
        expect(ID);
    } else if (token.token_type == NUM) {
        loc = next_available;
        mem[next_available] = stoi(token.lexeme);
        next_available++;
        expect(NUM);
    } else {
        cerr << "Error: OUTPUT expects variable or number at line "
             << token.line_no << endl;
        exit(1);
    }
    expect(SEMICOLON);
    InstructionNode* node = new InstructionNode;
    node->type = OUT;
    node->next = nullptr;
    node->output_inst.var_loc = loc;
    return node;
}

InstructionNode* parse_if_statement() {
    expect(IF);
    int op1, op2;
    ConditionalOperatorType cond;
    parse_condition_expr(op1, op2, cond);

    InstructionNode* ifNode = new InstructionNode;
    ifNode->type = CJMP;
    ifNode->next = nullptr;
    ifNode->cjmp_inst.condition_op = cond;
    ifNode->cjmp_inst.op1_loc = op1;
    ifNode->cjmp_inst.op2_loc = op2;
    ifNode->cjmp_inst.target = nullptr;

    InstructionNode* thenPart = parse_code_block();
    InstructionNode* endNode = makeNoop();

    ifNode->cjmp_inst.target = endNode;
    ifNode->next = linkInstructions(thenPart, endNode);

    return ifNode;
}

InstructionNode* parse_while_statement() {
    expect(WHILE);
    InstructionNode* whileNode = new InstructionNode;
    whileNode->type = CJMP;
    whileNode->next = nullptr;
    whileNode->cjmp_inst.target = nullptr;

    int op1, op2;
    ConditionalOperatorType cond;
    parse_condition_expr(op1, op2, cond);
    whileNode->cjmp_inst.condition_op = cond;
    whileNode->cjmp_inst.op1_loc = op1;
    whileNode->cjmp_inst.op2_loc = op2;

    InstructionNode* loopBody = parse_code_block();
    InstructionNode* loopBack = new InstructionNode;
    loopBack->type = JMP;
    loopBack->next = nullptr;
    loopBack->jmp_inst.target = whileNode;

    InstructionNode* afterWhile = makeNoop();
    InstructionNode* bodyWithBack = linkInstructions(loopBody, loopBack);
    whileNode->next = linkInstructions(bodyWithBack, afterWhile);
    whileNode->cjmp_inst.target = afterWhile;
    return whileNode;
}

InstructionNode* parse_switch_statement() {
    expect(SWITCH);
    string name = token.lexeme;
    if (varTable.find(name) == varTable.end()) {
        cerr << "Error: Switch variable '" << name << "' undeclared." << endl;
        exit(1);
    }
    int switchVarLoc = varTable[name];
    expect(ID);
    expect(LBRACE);

    InstructionNode* switchExit = makeNoop();
    InstructionNode* head = nullptr;
    InstructionNode* lastCase = nullptr;

    while (token.token_type == CASE) {
        expect(CASE);
        int caseVal = stoi(token.lexeme);
        expect(NUM);
        expect(COLON);
        InstructionNode* body = parse_code_block();
        InstructionNode* caseEnd = new InstructionNode;
        caseEnd->type = JMP;
        caseEnd->jmp_inst.target = switchExit;
        caseEnd->next = switchExit;
        InstructionNode* fullBody = linkInstructions(body, caseEnd);
        int constLoc = next_available;
        mem[next_available] = caseVal;
        next_available++;
        InstructionNode* caseCheck = new InstructionNode;
        caseCheck->type = CJMP;
        caseCheck->cjmp_inst.condition_op = CONDITION_NOTEQUAL;
        caseCheck->cjmp_inst.op1_loc = switchVarLoc;
        caseCheck->cjmp_inst.op2_loc = constLoc;
        caseCheck->cjmp_inst.target = fullBody;
        caseCheck->next = nullptr;
        if (!head) {
            head = caseCheck;
            lastCase = caseCheck;
        } else {
            lastCase->next = caseCheck;
            lastCase = caseCheck;
        }
    }

    if (token.token_type == DEFAULT) {
        expect(DEFAULT);
        expect(COLON);
        InstructionNode* defBody = parse_code_block();
        InstructionNode* defJmp = new InstructionNode;
        defJmp->type = JMP;
        defJmp->jmp_inst.target = switchExit;
        defJmp->next = switchExit;
        InstructionNode* defSection = linkInstructions(defBody, defJmp);
        if (!head) {
            head = defSection;
        } else {
            lastCase->next = defSection;
        }
    } else {
        if (lastCase) lastCase->next = switchExit;
        else head = switchExit;
    }
    expect(RBRACE);
    return head;
}

InstructionNode* parse_for_statement() {
    expect(FOR);
    expect(LPAREN);
    InstructionNode* init = parse_assign_statement();
    int left, right;
    ConditionalOperatorType cond;
    parse_condition_expr(left, right, cond);
    expect(SEMICOLON);
    InstructionNode* update = parse_assign_statement();
    expect(RPAREN);
    InstructionNode* body = parse_code_block();

    InstructionNode* cjmp = new InstructionNode;
    cjmp->type = CJMP;
    cjmp->next = nullptr;
    cjmp->cjmp_inst.op1_loc = left;
    cjmp->cjmp_inst.op2_loc = right;
    cjmp->cjmp_inst.condition_op = cond;
    InstructionNode* exit = makeNoop();
    cjmp->cjmp_inst.target = exit;

    InstructionNode* jmpBack = new InstructionNode;
    jmpBack->type = JMP;
    jmpBack->next = nullptr;
    jmpBack->jmp_inst.target = cjmp;

    InstructionNode* block = linkInstructions(body, update);
    block = linkInstructions(block, jmpBack);
    cjmp->next = block;

    return linkInstructions(init, linkInstructions(cjmp, exit));
}

void parse_condition_expr(int &left, int &right, ConditionalOperatorType &op) {
    left = parse_value();
    if (token.token_type == GREATER) {
        op = CONDITION_GREATER;
        expect(GREATER);
    } else if (token.token_type == LESS) {
        op = CONDITION_LESS;
        expect(LESS);
    } else if (token.token_type == NOTEQUAL) {
        op = CONDITION_NOTEQUAL;
        expect(NOTEQUAL);
    } else {
        cerr << "Error: Condition operator expected at line "
             << token.line_no << endl;
        exit(1);
    }
    right = parse_value();
}

int parse_value() {
    int loc;
    if (token.token_type == NUM) {
        loc = next_available;
        mem[next_available] = stoi(token.lexeme);
        next_available++;
        expect(NUM);
    } else if (token.token_type == ID) {
        string name = token.lexeme;
        if (varTable.find(name) == varTable.end()) {
            cerr << "Error: Variable '" << name << "' undeclared." << endl;
            exit(1);
        }
        loc = varTable[name];
        expect(ID);
    } else {
        cerr << "Error: Expected primary (number or variable) at line "
             << token.line_no << endl;
        exit(1);
    }
    return loc;
}

void parse_input_values() {
    while (token.token_type == NUM) {
        int value = stoi(token.lexeme);
        inputs.push_back(value);
        expect(NUM);
    }
}

InstructionNode* parse_Generate_Intermediate_Representation() {
    fetchNextToken();
    InstructionNode* program = parse_program();
    return program;
}
