//should be compiles as g++ -std=c++17 evaluate_amps_control_macro.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <set>
#include <regex>
#include <sstream>
#include <iomanip>   // For std::setw
#include <filesystem>
#include <type_traits>
#include <cctype>
#include <stdexcept>

using namespace std;
namespace fs = std::filesystem;

// Output type sizeof()
void OutputSizeof() {
    std::cout << std::left << std::setw(15) << "Type" 
              << std::right << std::setw(10) << "Size (bytes)" << std::endl;
    std::cout << std::string(25, '-') << std::endl;

    // Integer types
    std::cout << std::left << std::setw(15) << "char" << std::right << std::setw(10) << sizeof(char) << std::endl;
    std::cout << std::left << std::setw(15) << "short" << std::right << std::setw(10) << sizeof(short) << std::endl;
    std::cout << std::left << std::setw(15) << "int" << std::right << std::setw(10) << sizeof(int) << std::endl;
    std::cout << std::left << std::setw(15) << "long" << std::right << std::setw(10) << sizeof(long) << std::endl;
    std::cout << std::left << std::setw(15) << "long long" << std::right << std::setw(10) << sizeof(long long) << std::endl;

    // Floating-point types
    std::cout << std::left << std::setw(15) << "float" << std::right << std::setw(10) << sizeof(float) << std::endl;
    std::cout << std::left << std::setw(15) << "double" << std::right << std::setw(10) << sizeof(double) << std::endl;
    std::cout << std::left << std::setw(15) << "long double" << std::right << std::setw(10) << sizeof(long double) << std::endl;

    // Boolean and void
    std::cout << std::left << std::setw(15) << "bool" << std::right << std::setw(10) << sizeof(bool) << std::endl;

    // Wide character types
    std::cout << std::left << std::setw(15) << "wchar_t" << std::right << std::setw(10) << sizeof(wchar_t) << std::endl;
    std::cout << std::left << std::setw(15) << "char16_t" << std::right << std::setw(10) << sizeof(char16_t) << std::endl;
    std::cout << std::left << std::setw(15) << "char32_t" << std::right << std::setw(10) << sizeof(char32_t) << std::endl;
}

// Helper class to parse and evaluate expressions
class ExpressionParser {
public:
    ExpressionParser(const string& expr) : expression(expr), pos(0) {}

    double parse() {
        double result = parseOrExpression();
        skipWhitespace();
        if (pos != expression.length()) {
            throw runtime_error("Unexpected characters at end of expression");
        }
        return result;
    }

private:
    string expression;
    size_t pos;

    void skipWhitespace() {
        while (pos < expression.length() && isspace(expression[pos])) {
            pos++;
        }
    }

    // Parse OR expressions
    double parseOrExpression() {
        double left = parseAndExpression();
        while (true) {
            skipWhitespace();
            if (match("||")) {
                double right = parseAndExpression();
                left = (left || right) ? 1.0 : 0.0;
            }
            else {
                break;
            }
        }
        return left;
    }

    // Parse AND expressions
    double parseAndExpression() {
        double left = parseEqualityExpression();
        while (true) {
            skipWhitespace();
            if (match("&&")) {
                double right = parseEqualityExpression();
                left = (left && right) ? 1.0 : 0.0;
            }
            else {
                break;
            }
        }
        return left;
    }

    // Parse Equality expressions (==, !=)
    double parseEqualityExpression() {
        double left = parseRelationalExpression();
        while (true) {
            skipWhitespace();
            if (match("==")) {
                double right = parseRelationalExpression();
                left = (left == right) ? 1.0 : 0.0;
            }
            else if (match("!=")) {
                double right = parseRelationalExpression();
                left = (left != right) ? 1.0 : 0.0;
            }
            else {
                break;
            }
        }
        return left;
    }

    // Parse Relational expressions (<, >, <=, >=)
    double parseRelationalExpression() {
        double left = parseAdditiveExpression();
        while (true) {
            skipWhitespace();
            if (match("<=")) {
                double right = parseAdditiveExpression();
                left = (left <= right) ? 1.0 : 0.0;
            }
            else if (match(">=")) {
                double right = parseAdditiveExpression();
                left = (left >= right) ? 1.0 : 0.0;
            }
            else if (match("<")) {
                double right = parseAdditiveExpression();
                left = (left < right) ? 1.0 : 0.0;
            }
            else if (match(">")) {
                double right = parseAdditiveExpression();
                left = (left > right) ? 1.0 : 0.0;
            }
            else {
                break;
            }
        }
        return left;
    }

    // Parse Additive expressions (+, -)
    double parseAdditiveExpression() {
        double left = parseMultiplicativeExpression();
        while (true) {
            skipWhitespace();
            if (match("+")) {
                double right = parseMultiplicativeExpression();
                left += right;
            }
            else if (match("-")) {
                double right = parseMultiplicativeExpression();
                left -= right;
            }
            else {
                break;
            }
        }
        return left;
    }

    // Parse Multiplicative expressions (*, /, %)
    double parseMultiplicativeExpression() {
        double left = parseUnaryExpression();
        while (true) {
            skipWhitespace();
            if (match("*")) {
                double right = parseUnaryExpression();
                left *= right;
            }
            else if (match("/")) {
                double right = parseUnaryExpression();
                if (right == 0) {
                    throw runtime_error("Division by zero");
                }
                left /= right;
            }
            else if (match("%")) {
                double right = parseUnaryExpression();
                if (right == 0) {
                    throw runtime_error("Modulo by zero");
                }
                left = static_cast<int>(left) % static_cast<int>(right);
            }
            else {
                break;
            }
        }
        return left;
    }

    // Parse Unary expressions (!, +, -)
    double parseUnaryExpression() {
        skipWhitespace();
        if (match("!")) {
            double operand = parseUnaryExpression();
            return (!operand) ? 1.0 : 0.0;
        }
        else if (match("+")) {
            return parseUnaryExpression();
        }
        else if (match("-")) {
            return -parseUnaryExpression();
        }
        else {
            return parsePrimaryExpression();
        }
    }

    // Parse Primary expressions (numbers, parentheses)
    double parsePrimaryExpression() {
        skipWhitespace();
        if (match("(")) {
            double expr = parseOrExpression();
            if (!match(")")) {
                throw runtime_error("Expected ')'");
            }
            return expr;
        }
        else {
            return parseNumber();
        }
    }

    // Parse a number (integer or floating-point)
    double parseNumber() {
        skipWhitespace();
        size_t start = pos;
        bool hasDecimal = false;
        while (pos < expression.length() && (isdigit(expression[pos]) || expression[pos] == '.')) {
            if (expression[pos] == '.') {
                if (hasDecimal) {
                    throw runtime_error("Invalid number format");
                }
                hasDecimal = true;
            }
            pos++;
        }
        if (start == pos) {
            throw runtime_error("Expected number at position " + to_string(pos));
        }
        string numberStr = expression.substr(start, pos - start);
        return stod(numberStr);
    }

    // Utility function to match a string at the current position
    bool match(const string& token) {
        skipWhitespace();
        if (expression.compare(pos, token.length(), token) == 0) {
            pos += token.length();
            return true;
        }
        return false;
    }
};


// Global data structures for macro definitions and file tracking
unordered_map<string, double> macro_values;       // Stores macros with numeric values
unordered_map<string, bool> undefined_macros;     // Stores macros defined without values
set<fs::path> processed_files;                    // Tracks files already processed to avoid duplication

unordered_set<string> TrapMacroTable;             // Stores macros that should trigger TrapMacro() when processed
bool verbose_mode = false;                        // Controls debug output

// Debug print function
void debugPrint(const string& message) {
    if (verbose_mode) {
        cout << "[DEBUG] " << message << endl;
    }
}

// Condition structure to manage conditional compilation state
struct Condition {
    bool isActive;   // If the current branch is active
    bool hasElse;    // If the #else branch has been encountered
};

// Debug function to act as a breakpoint location for specific macros
void TrapMacro(const string& macroName) {
    cout << "[DEBUG] Trap hit for macro: " << macroName << endl;
    // Place a debugger breakpoint here if needed
}

// Function to get the size of common C++ types, including compound types
double getSizeOfType(const string &type) {
    if (type == "int") return sizeof(int);
    if (type == "double") return sizeof(double);
    if (type == "float") return sizeof(float);
    if (type == "char") return sizeof(char);
    if (type == "bool") return sizeof(bool);
    if (type == "long") return sizeof(long);
    if (type == "short") return sizeof(short);
    if (type == "long int") return sizeof(long);
    if (type == "long long") return sizeof(long long);
    if (type == "unsigned int") return sizeof(unsigned int);
    if (type == "unsigned char") return sizeof(unsigned char);
    if (type == "unsigned long") return sizeof(unsigned long);
    if (type == "unsigned long int") return sizeof(unsigned long);
    if (type == "unsigned long long") return sizeof(unsigned long long);
    if (type == "unsigned short") return sizeof(unsigned short);

    cerr << "Unsupported type for sizeof: " << type << endl;
    return -1; // Return -1 for unsupported types
}

// Substitute macros with their values in an expression and process sizeof(type)
string substituteMacros(const string &expression);

// Evaluate arithmetic operations without parentheses
double evaluateSimpleExpression(const std::string& expression) {
    std::istringstream iss(expression);
    std::vector<double> numbers;
    std::vector<char> operators;
    
    // Read first number
    double value;
    if (!(iss >> value)) {
        std::cerr << "Failed to evaluate simple expression: " << expression << std::endl;
        return 0;
    }
    numbers.push_back(value);
    
    // Read operator-number pairs
    char op;
    while (iss >> op >> value) {
        operators.push_back(op);
        numbers.push_back(value);
    }
    
    // First pass: handle multiplication and division
    for (size_t i = 0; i < operators.size(); ++i) {
        if (operators[i] == '*' || operators[i] == '/') {
            if (operators[i] == '*') {
                numbers[i] = numbers[i] * numbers[i + 1];
            } else if (operators[i] == '/' && numbers[i + 1] != 0) {
                numbers[i] = numbers[i] / numbers[i + 1];
            } else {
                std::cerr << "Division by zero detected" << std::endl;
                return 0;
            }
            // Mark the processed number as invalid using a special value
            numbers[i + 1] = 0;
            // Mark the operator as processed
            operators[i] = 'x';
        }
    }
    
    // Second pass: handle addition and subtraction
    double result = numbers[0];
    for (size_t i = 0; i < operators.size(); ++i) {
        if (operators[i] != 'x') {  // Skip processed operators
            // Skip numbers that were part of multiplication/division
            double nextNum = numbers[i + 1];
            if (operators[i] == '+') {
                result += nextNum;
            } else if (operators[i] == '-') {
                result -= nextNum;
            }
        }
    }
    
    return result;
}

// Evaluate expressions, handling arithmetic and parentheses
double evaluateExpression(const string& expr) {
    ExpressionParser parser(expr);
    try {
        double result = parser.parse();
        return result;
    }
    catch (const exception& e) {
        cerr << "Error evaluating expression '" << expr << "': " << e.what() << endl;
        return 0.0; // Default to false on error
    }
}

// Substitute macros with their values in an expression and handle sizeof(type)
string substituteMacros(const string &expression) {
    string substituted = expression;

    // Replace macros with their values
    for (const auto &macro : macro_values) {
        regex macroRegex("\\b" + macro.first + "\\b"); // match whole words
        substituted = regex_replace(substituted, macroRegex, to_string(macro.second));
    }

    // Handle sizeof(type) expressions
    regex sizeofRegex(R"(sizeof\s*\(\s*([a-zA-Z_]\w*(\s+[a-zA-Z_]\w*)*)\s*\))");
    smatch match;
    while (regex_search(substituted, match, sizeofRegex)) {
        string type = match[1].str();
        double size = getSizeOfType(type);
        if (size != -1) {
            substituted.replace(match.position(0), match.length(0), to_string(size));
            if (verbose_mode==true) cout << "Substituted sizeof(" << type << ") with " << size << endl; // Debug print
        } else {
            cerr << "Failed to evaluate sizeof for type: " << type << endl;
            return "0"; // Return 0 for unsupported types
        }
    }

    if (verbose_mode==true) cout << "Expression after substitution: " << substituted << endl; // Debug print
    return substituted;
}

// Parse and process a file, respecting conditional compilation and macros
void parseFile(const fs::path &filename, stack<Condition> &conditionStack);

// Process a single line of code, handling all preprocessor directives and conditional compilation
void processLine(const string &line, stack<Condition> &conditionStack, const fs::path &baseDir) {
    // Regex patterns for matching preprocessor directives
    regex defineRegex(R"(#define\s+(\w+)(.*))");
    regex undefRegex(R"(#undef\s+(\w+))");
    regex includeRegex(R"(#include\s+\"(.+)\")");
    regex ifRegex(R"(#if\s+(.+))");
    regex ifdefRegex(R"(#ifdef\s+(\w+))");
    regex ifndefRegex(R"(#ifndef\s+(\w+))");
    regex elseRegex(R"(#else)");
    regex endifRegex(R"(#endif)");

    smatch match;
    if (verbose_mode==true) cout << "Processing line: " << line << endl;  // Debug print

    // Ignore processing if the current condition stack has an inactive condition
    if (!conditionStack.empty() && !conditionStack.top().isActive) {
        if (regex_match(line, endifRegex)) {
            conditionStack.pop(); // End conditional block
        } else if (regex_match(line, elseRegex)) {
            Condition &top = conditionStack.top();
            if (!top.hasElse) {
                top.isActive = !top.isActive; // Toggle activity for #else
                top.hasElse = true;
            }
        }
        return;
    }

    // Check if the current line defines a macro
    if (regex_match(line, match, defineRegex)) {
        string macro = match[1].str();
        string value = match[2].str();

        // Check if the macro is in TrapMacroTable
        if (TrapMacroTable.find(macro) != TrapMacroTable.end()) {
            TrapMacro(macro); // Call trap function if macro is in TrapMacroTable
        }

        if (value.empty()) {
            if (verbose_mode==true) cout << "Found macro without value: " << macro << endl;  // Debug print
            undefined_macros[macro] = true;
        } else {
            if (verbose_mode==true) cout << "Defining macro: " << macro << " with expression: " << value << endl;  // Debug print
            double evaluatedValue = evaluateExpression(substituteMacros(value));
            macro_values[macro] = evaluatedValue;
            if (verbose_mode==true) cout << "Macro " << macro << " evaluated to: " << evaluatedValue << endl;  // Debug print
        }
    } else if (regex_match(line, match, undefRegex)) {
        string macro = match[1].str();
        if (verbose_mode==true) cout << "Undefining macro: " << macro << endl;  // Debug print
        macro_values.erase(macro);
        undefined_macros.erase(macro);
    } else if (regex_match(line, match, includeRegex)) {
        string includeFile = match[1].str();
        fs::path includePath = baseDir / includeFile;
        if (processed_files.find(includePath) == processed_files.end()) {
            if (verbose_mode==true) cout << "Including file: " << includeFile << endl;  // Debug print
            parseFile(includePath, conditionStack);
        } else {
            cout << "File " << includeFile << " already processed. Skipping to avoid recursion." << endl;
        }
    } else if (regex_match(line, match, ifRegex)) {
        bool condition = evaluateExpression(substituteMacros(match[1].str())) != 0;
        conditionStack.push({condition, false});
        if (verbose_mode==true) cout << "Evaluating #if condition (" << match[1].str() << ") as " << (condition ? "true" : "false") << endl;  // Debug print
    } else if (regex_match(line, match, ifdefRegex)) {
        string macro = match[1].str();
        bool condition = macro_values.find(macro) != macro_values.end();
        conditionStack.push({condition, false});
        if (verbose_mode==true) cout << "Evaluating #ifdef " << macro << " as " << (condition ? "true" : "false") << endl;  // Debug print
    } else if (regex_match(line, match, ifndefRegex)) {
        string macro = match[1].str();
        bool condition = macro_values.find(macro) == macro_values.end();
        conditionStack.push({condition, false});
        if (verbose_mode==true) cout << "Evaluating #ifndef " << macro << " as " << (condition ? "true" : "false") << endl;  // Debug print
    } else if (regex_match(line, elseRegex)) {
        Condition &top = conditionStack.top();
        if (!top.hasElse) {
            top.isActive = !top.isActive;
            top.hasElse = true;
            if (verbose_mode==true) cout << "Toggling condition for #else as " << top.isActive << endl;  // Debug print
        }
    } else if (regex_match(line, endifRegex)) {
        if (verbose_mode==true) cout << "Ending conditional block with #endif" << endl;  // Debug print
        if (!conditionStack.empty()) {
            conditionStack.pop();
        }
    }
}

// Parse each file, considering conditional directives and avoiding re-processing
void parseFile(const fs::path &filename, stack<Condition> &conditionStack) {
    if (processed_files.find(filename) != processed_files.end()) {
        cout << "File " << filename << " already processed. Skipping." << endl;
        return;
    }

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << filename << endl;
        return;
    }

    processed_files.insert(filename);  // Mark this file as processed

    string line, completeLine;
    while (getline(file, line)) {
       // Remove leading spaces from the line
       line.erase(line.begin(), find_if(line.begin(), line.end(), [](unsigned char ch) {
         return !isspace(ch);
       }));

        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty() && line.back() == '\\') {
            line.pop_back();
            completeLine += line;
            continue;
        } else {
            completeLine += line;
        }

        processLine(completeLine, conditionStack, filename.parent_path());

        completeLine.clear();
    }

    file.close();
}

// Print macros that meet specific conditions
void printFilteredMacros() {
    vector<pair<string, double>> filteredMacros;

    // Collect macros matching specific criteria
    for (const auto &macro : macro_values) {
        if (macro.first == "_PIC_MODE_ON_" || macro.first == "_PIC_MODE_OFF_" || 
            macro.first.find("OFFSET") != string::npos) {
            filteredMacros.emplace_back(macro.first, macro.second);
        }
    }

    // Sort macros alphabetically by name
    std::sort(filteredMacros.begin(), filteredMacros.end());

    // Print the sorted macros
    cout << "\nFiltered Macros (Alphabetically Sorted):\n";
    cout << setw(30) << left << "Macro Name" << "Value" << endl;
    cout << string(50, '-') << endl;
    for (const auto &macro : filteredMacros) {
        cout << setw(30) << left << macro.first << macro.second << endl;
    }
}

// Structure to hold macro name and its offset value for sorting
struct OffsetMacro {
    std::string name;
    double offset;
};

// Function to create, sort, and print a table of macros with "OFFSET" in their names
void createOffsetTable() {
    vector<OffsetMacro> offsetMacros;

    // Collect all macros with "OFFSET" in their name and retrieve their values
    for (const auto& macro : macro_values) {
        if (macro.first.find("OFFSET") != std::string::npos) {
            offsetMacros.push_back({macro.first, macro.second});
        }
    }

    // Sort the collected macros by the offset value in increasing order
    std::sort(offsetMacros.begin(), offsetMacros.end(), [](const OffsetMacro& a, const OffsetMacro& b) {
        return a.offset < b.offset;
    });

    // Print the sorted table of offset macros
    cout << "\nOffset Macros Table (Sorted by Offset Value):\n";
    cout << setw(30) << left << "Macro Name" << "Offset Value" << endl;
    cout << string(50, '-') << endl;
    for (const auto& macro : offsetMacros) {
        cout << setw(30) << left << macro.name << macro.offset << endl;
    }
}

int main(int argc, char *argv[]) {
    // Check for help flags
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        
	
	if (arg == "-verbose" || arg == "-v") {
            verbose_mode = true;
            debugPrint("Verbose mode enabled");
        } 
	else if (arg == "-help" || arg == "-h") {
            cout << "Usage: " << argv[0] << " <file1> <file2> ... [options]\n";
	    cout << "Usage: " << argv[0] << " pic.h picGlobal.dfn picParticleDataMacro.h\n\n";
            cout << "This program processes C++ source files and headers to evaluate macros.\n";
            cout << "It supports multi-level #include directives, conditional compilation with #if, #else, #endif,\n";
            cout << "and tracks macro definitions across files.\n\n";
            cout << "Options:\n";
            cout << "  -help, -h       Show this help message.\n\n";
	    cout << "  -verbose, -v    Enable verbose debug output.\n\n";
            cout << "Usage Example:\n";
            cout << "  " << argv[0] << " file1.h file2.h\n";
	    cout << "  " << argv[0] << " ../general/global.dfn ../general/global.h pic.h picGlobal.dfn picParticleDataMacro.h\n\n";
            cout << "Detailed Explanation:\n";
            cout << "  1. The program processes files in the specified order, and applies macros defined in\n";
            cout << "     earlier files to subsequent files.\n";
            cout << "  2. Each macro definition (#define) is stored and evaluated if it includes an arithmetic expression.\n";
            cout << "  3. Conditional directives (#if, #else, #endif) control whether specific macros are processed.\n";
            cout << "  4. Multi-line macros ending in '\\' are supported.\n\n";
            cout << "TrapMacroTable:\n";
            cout << "  - A set of macros that trigger a debug function 'TrapMacro' when processed. This is useful for\n";
            cout << "    debugging specific macros.\n";
            cout << "  - To add macros to this table, update the 'TrapMacroTable' in 'main()' before running.\n\n";
            cout << "Filtered Macro Printing:\n";
            cout << "  - After processing all macros, the program filters and re-prints specific macros that match:\n";
            cout << "    '_PIC_MODE_ON_', '_PIC_MODE_OFF_', or contain 'OFFSET' in their names.\n\n";
            cout << "Output:\n";
            cout << "  - Macro Values Table: Displays all evaluated macros with their values.\n";
            cout << "  - Macros Defined Without Value: Lists macros defined without a specific value.\n";
            cout << "  - Filtered Macros: Shows filtered macros based on specific criteria.\n\n";
            cout << "Example with TrapMacroTable:\n";
            cout << "  Add specific macros to TrapMacroTable (e.g., '_PIC_PARTICLE_DATA__NEXT_OFFSET_')\n";
            cout << "  in 'main()' for additional debug output during processing.\n\n";
            return 0;
        }
    }

    // Initialize TrapMacroTable with macros to test in the debugger
    TrapMacroTable.insert("_PIC_PARTICLE_DATA__NEXT_OFFSET_");
    TrapMacroTable.insert("_PIC_PARTICLE_DATA__VELOCITY_OFFSET_");
    TrapMacroTable.insert("_PIC_PARTICLE_DATA__WEIGHT_CORRECTION_OFFSET_"); 
    TrapMacroTable.insert("_PIC_PARTICLE_DATA__MAGNETIC_MOMENT_OFFSET_");
    TrapMacroTable.insert("_USE_MAGNETIC_MOMENT_");
    TrapMacroTable.insert("TEST");

    // Ensure there are files to process
    if (argc < 2) {
        cerr << "Error: No input files specified. Use -help for usage information.\n";
        return 1;
    }

    stack<Condition> conditionStack;
    conditionStack.push({true, false});  // Start with a true condition for the outer scope

    // Process each file specified in the argument line
    for (int i = 1; i < argc; ++i) {
        fs::path currentFile = argv[i];
        parseFile(currentFile, conditionStack);
    }

    int max_width = 0;
    for (const auto &macro : macro_values) {
        max_width = max(max_width, static_cast<int>(macro.first.length()));
    }
    for (const auto &macro : undefined_macros) {
        max_width = max(max_width, static_cast<int>(macro.first.length()));
    }
    max_width += 2;

    // Display macro values table
    cout << "Macro Values Table:\n";
    for (const auto &macro : macro_values) {
        cout << setw(max_width) << left << macro.first << ": " << macro.second << endl;
    }

    // Display macros defined without a value
    cout << "\nMacros Defined Without Value:\n";
    for (const auto &macro : undefined_macros) {
        cout << setw(max_width) << left << macro.first << endl;
    }

    // Call the filtered print function to display specific macros
    printFilteredMacros();

    // Calls the new function to print sorted offset macros
    createOffsetTable(); 

    // Output sizeof 
    OutputSizeof();

    return 0;
}

