#include "MathParser.h"

#include <QStringList>
#include <cmath>
#include <functional>

namespace {
enum class TokenType {
    Number,
    Identifier,
    Plus,
    Minus,
    Multiply,
    Divide,
    Power,
    LeftParen,
    RightParen,
    End
};

struct Token
{
    TokenType type = TokenType::End;
    QString text;
    double number = 0.0;
};

class Lexer
{
public:
    explicit Lexer(const QString &input)
        : m_input(input)
    {
    }

    QList<Token> tokenize(QString *error)
    {
        QList<Token> tokens;
        while (m_index < m_input.size()) {
            const QChar ch = m_input.at(m_index);
            if (ch.isSpace()) {
                ++m_index;
                continue;
            }
            if (ch.isDigit() || ch == '.') {
                tokens.append(readNumber());
                continue;
            }
            if (ch.isLetter() || ch == '_') {
                tokens.append(readIdentifier());
                continue;
            }
            switch (ch.unicode()) {
            case '+':
                tokens.append({ TokenType::Plus, QStringLiteral("+"), 0.0 });
                ++m_index;
                break;
            case '-':
                tokens.append({ TokenType::Minus, QStringLiteral("-"), 0.0 });
                ++m_index;
                break;
            case '*':
                tokens.append({ TokenType::Multiply, QStringLiteral("*"), 0.0 });
                ++m_index;
                break;
            case '/':
                tokens.append({ TokenType::Divide, QStringLiteral("/"), 0.0 });
                ++m_index;
                break;
            case '^':
                tokens.append({ TokenType::Power, QStringLiteral("^"), 0.0 });
                ++m_index;
                break;
            case '(':
                tokens.append({ TokenType::LeftParen, QStringLiteral("("), 0.0 });
                ++m_index;
                break;
            case ')':
                tokens.append({ TokenType::RightParen, QStringLiteral(")"), 0.0 });
                ++m_index;
                break;
            default:
                if (error)
                    *error = QStringLiteral("Unsupported character: %1").arg(ch);
                return {};
            }
        }
        tokens.append({ TokenType::End, QString(), 0.0 });
        return tokens;
    }

private:
    Token readNumber()
    {
        const int start = m_index;
        bool hasDot = false;
        while (m_index < m_input.size()) {
            const QChar ch = m_input.at(m_index);
            if (ch.isDigit()) {
                ++m_index;
            } else if (ch == '.' && !hasDot) {
                hasDot = true;
                ++m_index;
            } else {
                break;
            }
        }
        Token token;
        token.type = TokenType::Number;
        token.text = m_input.mid(start, m_index - start);
        token.number = token.text.toDouble();
        return token;
    }

    Token readIdentifier()
    {
        const int start = m_index;
        while (m_index < m_input.size() && (m_input.at(m_index).isLetterOrNumber() || m_input.at(m_index) == '_'))
            ++m_index;
        Token token;
        token.type = TokenType::Identifier;
        token.text = m_input.mid(start, m_index - start);
        return token;
    }

    QString m_input;
    int m_index = 0;
};

class Parser
{
public:
    Parser(const QList<Token> &tokens)
        : m_tokens(tokens)
    {
    }

    bool parse(double *result, QString *error)
    {
        m_error = error;
        if (!expression(result))
            return false;
        if (m_tokens.at(m_position).type != TokenType::End) {
            if (m_error)
                *m_error = QStringLiteral("Unexpected token after expression.");
            return false;
        }
        return true;
    }

private:
    bool expression(double *result)
    {
        double left = 0.0;
        if (!term(&left))
            return false;
        while (peek().type == TokenType::Plus || peek().type == TokenType::Minus) {
            const Token op = consume();
            double right = 0.0;
            if (!term(&right))
                return false;
            left = op.type == TokenType::Plus ? left + right : left - right;
        }
        *result = left;
        return true;
    }

    bool term(double *result)
    {
        double left = 0.0;
        if (!power(&left))
            return false;
        while (peek().type == TokenType::Multiply || peek().type == TokenType::Divide) {
            const Token op = consume();
            double right = 0.0;
            if (!power(&right))
                return false;
            if (op.type == TokenType::Divide && std::fabs(right) < 1e-15) {
                if (m_error)
                    *m_error = QStringLiteral("Division by zero.");
                return false;
            }
            left = op.type == TokenType::Multiply ? left * right : left / right;
        }
        *result = left;
        return true;
    }

    bool power(double *result)
    {
        double base = 0.0;
        if (!unary(&base))
            return false;
        if (peek().type == TokenType::Power) {
            consume();
            double exponent = 0.0;
            if (!power(&exponent))
                return false;
            *result = std::pow(base, exponent);
            return true;
        }
        *result = base;
        return true;
    }

    bool unary(double *result)
    {
        if (peek().type == TokenType::Plus) {
            consume();
            return unary(result);
        }
        if (peek().type == TokenType::Minus) {
            consume();
            double value = 0.0;
            if (!unary(&value))
                return false;
            *result = -value;
            return true;
        }
        return primary(result);
    }

    bool primary(double *result)
    {
        const Token token = consume();
        if (token.type == TokenType::Number) {
            *result = token.number;
            return true;
        }
        if (token.type == TokenType::LeftParen) {
            double value = 0.0;
            if (!expression(&value))
                return false;
            if (consume().type != TokenType::RightParen) {
                if (m_error)
                    *m_error = QStringLiteral("Missing closing parenthesis.");
                return false;
            }
            *result = value;
            return true;
        }
        if (token.type == TokenType::Identifier) {
            const QString name = token.text.toLower();
            if (name == QStringLiteral("pi")) {
                *result = 3.14159265358979323846;
                return true;
            }
            if (name == QStringLiteral("e")) {
                *result = 2.71828182845904523536;
                return true;
            }
            if (peek().type == TokenType::LeftParen) {
                consume();
                double argument = 0.0;
                if (!expression(&argument))
                    return false;
                if (consume().type != TokenType::RightParen) {
                    if (m_error)
                        *m_error = QStringLiteral("Missing closing parenthesis.");
                    return false;
                }
                if (name == QStringLiteral("sin"))
                    *result = std::sin(argument);
                else if (name == QStringLiteral("cos"))
                    *result = std::cos(argument);
                else if (name == QStringLiteral("tan"))
                    *result = std::tan(argument);
                else if (name == QStringLiteral("sqrt"))
                    *result = std::sqrt(argument);
                else if (name == QStringLiteral("abs"))
                    *result = std::fabs(argument);
                else if (name == QStringLiteral("log") || name == QStringLiteral("ln"))
                    *result = std::log(argument);
                else {
                    if (m_error)
                        *m_error = QStringLiteral("Unknown function: %1").arg(name);
                    return false;
                }
                return true;
            }
            if (m_error)
                *m_error = QStringLiteral("Unknown identifier: %1").arg(name);
            return false;
        }
        if (m_error)
            *m_error = QStringLiteral("Expected a number, function, or parentheses.");
        return false;
    }

    const Token &peek() const
    {
        return m_tokens.at(m_position);
    }

    Token consume()
    {
        return m_tokens.at(m_position++);
    }

    QList<Token> m_tokens;
    int m_position = 0;
    QString *m_error = nullptr;
};
}

bool MathParser::evaluate(const QString &expression, double *result, QString *error)
{
    if (!result)
        return false;
    Lexer lexer(expression);
    QString lexError;
    const QList<Token> tokens = lexer.tokenize(&lexError);
    if (!lexError.isEmpty()) {
        if (error)
            *error = lexError;
        return false;
    }
    if (tokens.isEmpty()) {
        if (error)
            *error = QStringLiteral("Empty expression.");
        return false;
    }
    Parser parser(tokens);
    return parser.parse(result, error);
}

