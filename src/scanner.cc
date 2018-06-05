#include <tree_sitter/parser.h>
#include <vector>
#include <cwctype>
#include <cassert>

namespace {

    using std::vector;

    enum TokenType {
        NEWLINE,
        INDENT,
        DEDENT,
    };

    struct Scanner {
        Scanner() {
            deserialize(NULL, 0);
        }

        unsigned serialize(char *buffer) {
            size_t i = 0;
            buffer[i++] = queued_dedent_count;

            vector<uint16_t>::iterator
            iter = indent_length_stack.begin() + 1,
            end = indent_length_stack.end();
            for (; iter != end && i < TREE_SITTER_SERIALIZATION_BUFFER_SIZE; ++iter) {
                buffer[i++] = *iter;
            }

            return i;
        }

        void deserialize(const char *buffer, unsigned length) {
            queued_dedent_count = 0;
            indent_length_stack.clear();
            indent_length_stack.push_back(0);

            if (length > 0) {
                size_t i = 0;
                queued_dedent_count = buffer[i++];
                while (i < length) indent_length_stack.push_back(buffer[i++]);
            }
        }

        void advance(TSLexer *lexer) {
            lexer->advance(lexer, false);
        }

        void printValidSymbols(const bool *valid_symbols) {
            if (valid_symbols[DEDENT]) {
                printf("<dedent>\n");
            } else if (valid_symbols[INDENT]) {
                printf("<indent>\n");
            } else if (valid_symbols[NEWLINE]) {
                printf("<newline>\n");
            } else {
                printf("<------->\n");
            }
        }

        void skipSpaces(TSLexer *lexer) {
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\r') {
                lexer->advance(lexer, true);
            }
        }

        // returns True if skipped newline
        bool skipJunk(TSLexer *lexer) {
            bool skippedNewline = false;
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                if (lexer->lookahead == '\n')
                    skippedNewline = true;
                lexer->advance(lexer, true);
            }
            return skippedNewline;
        }

        void advanceSpaces(TSLexer *lexer) {
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\r') {
                lexer->advance(lexer, false);
            }
        }

        void skipNewline(TSLexer *lexer) {
            while (lexer->lookahead == '\n') {
                lexer->advance(lexer, true);
            }
        }

        bool scan(TSLexer *lexer, const bool *valid_symbols) {
            bool skippedNewline = false;

            if (valid_symbols[DEDENT] && queued_dedent_count > 0) {
                queued_dedent_count--;
                lexer->result_symbol = DEDENT;
                return true;
            }


            skippedNewline = skippedNewline || skipJunk(lexer);

            if (lexer->lookahead == 0) {
                if (valid_symbols[DEDENT] && indent_length_stack.size() > 1) {
                    indent_length_stack.pop_back();
                    lexer->result_symbol = DEDENT;
                    return true;
                }

                if (valid_symbols[NEWLINE]) {
                    lexer->result_symbol = NEWLINE;
                    return true;
                }

                return false;
            }

            skippedNewline = skippedNewline || skipJunk(lexer);
            lexer->mark_end(lexer);

            bool next_token_is_comment = false;
            uint32_t indent_length = lexer->get_column(lexer);

            if (!next_token_is_comment) {
                printf("\n\n[%c]    \t%d/%d\n", lexer->lookahead, indent_length, indent_length_stack.back());
                printValidSymbols(valid_symbols);

                if (skippedNewline)
                    printf("skipNewline\n");

                if (valid_symbols[NEWLINE] && skippedNewline && indent_length == indent_length_stack.back()) {
                    lexer->result_symbol = NEWLINE;
                    return true;
                }

                if (valid_symbols[INDENT] && indent_length > indent_length_stack.back()) {
                    indent_length_stack.push_back(indent_length);
                    lexer->result_symbol = INDENT;
                    return true;
                }

                if (indent_length < indent_length_stack.back()) {
                    indent_length_stack.pop_back();
                    while (indent_length < indent_length_stack.back()) {
                        indent_length_stack.pop_back();
                        queued_dedent_count++;
                    }

                    if (valid_symbols[DEDENT]) {
                        lexer->result_symbol = DEDENT;
                        return true;
                    } else {
                        queued_dedent_count++;
                    }
                }
            }
            //
            // if (valid_symbols[NEWLINE]) {
            //     printf("??????\n");
            //     lexer->result_symbol = NEWLINE;
            //     return true;
            // }

            return false;
        }

        vector<uint16_t> indent_length_stack;
        uint32_t queued_dedent_count;
    };

}

extern "C" {

    void *tree_sitter_agda_external_scanner_create() {
        return new Scanner();
    }

    bool tree_sitter_agda_external_scanner_scan(void *payload, TSLexer *lexer,
        const bool *valid_symbols) {
            Scanner *scanner = static_cast<Scanner *>(payload);
            return scanner->scan(lexer, valid_symbols);
        }

        unsigned tree_sitter_agda_external_scanner_serialize(void *payload, char *buffer) {
            Scanner *scanner = static_cast<Scanner *>(payload);
            return scanner->serialize(buffer);
        }

        void tree_sitter_agda_external_scanner_deserialize(void *payload,
            const char *buffer,
            unsigned length) {
                Scanner *scanner = static_cast<Scanner *>(payload);
                scanner->deserialize(buffer, length);
            }

            void tree_sitter_agda_external_scanner_destroy(void *payload) {
                Scanner *scanner = static_cast<Scanner *>(payload);
                delete scanner;
            }

        }
