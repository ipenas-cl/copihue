use serde::{Deserialize, Serialize};

/// Top-level template types
#[derive(Debug, Deserialize, Serialize)]
#[serde(untagged)]
pub enum Template {
    Program(ProgramTemplate),
    Function(FunctionTemplate),
}

/// Program template - simplest entry point
#[derive(Debug, Deserialize, Serialize)]
pub struct ProgramTemplate {
    pub name: String,
    #[serde(default)]
    pub description: Option<String>,
    pub actions: Vec<Action>,
}

/// Actions that a program can perform
#[derive(Debug, Deserialize, Serialize)]
#[serde(untagged)]
pub enum Action {
    /// Print a string to stdout
    Print(String),
}

/// Function template
#[derive(Debug, Deserialize, Serialize)]
pub struct FunctionTemplate {
    pub name: String,
    #[serde(default)]
    pub description: Option<String>,
    #[serde(default)]
    pub inputs: Vec<Parameter>,
    #[serde(default)]
    pub output: Option<TypeSpec>,
    pub implementation: Vec<Statement>,
}

/// Function parameter
#[derive(Debug, Deserialize, Serialize)]
pub struct Parameter {
    pub name: String,
    #[serde(rename = "type")]
    pub param_type: TypeSpec,
}

/// Type specifications
#[derive(Debug, Clone, Deserialize, Serialize)]
pub enum TypeSpec {
    #[serde(rename = "i32")]
    I32,
    #[serde(rename = "i64")]
    I64,
    #[serde(rename = "u32")]
    U32,
    #[serde(rename = "u64")]
    U64,
    #[serde(rename = "String")]
    String,
}

/// Statements
#[derive(Debug, Deserialize, Serialize)]
#[serde(tag = "type")]
pub enum Statement {
    Return { value: Expression },
}

/// Expressions
#[derive(Debug, Deserialize, Serialize)]
#[serde(untagged)]
pub enum Expression {
    Literal(Literal),
    Variable(String),
    BinaryOp {
        op: BinaryOperator,
        left: Box<Expression>,
        right: Box<Expression>,
    },
}

/// Literal values
#[derive(Debug, Deserialize, Serialize)]
#[serde(untagged)]
pub enum Literal {
    Integer(i64),
    String(String),
}

/// Binary operators
#[derive(Debug, Deserialize, Serialize)]
pub enum BinaryOperator {
    Add,
    Sub,
    Mul,
    Div,
}
