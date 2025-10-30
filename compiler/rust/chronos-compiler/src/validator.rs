use crate::parser::*;
use anyhow::{bail, Result};

pub fn validate_template(template: &Template) -> Result<()> {
    match template {
        Template::Program(prog) => validate_program(prog),
        Template::Function(func) => validate_function(func),
    }
}

fn validate_program(prog: &ProgramTemplate) -> Result<()> {
    // Name must not be empty
    if prog.name.is_empty() {
        bail!("Program name cannot be empty");
    }

    // Name must be valid identifier (alphanumeric + underscore)
    if !prog.name.chars().all(|c| c.is_alphanumeric() || c == '_') {
        bail!("Program name must be a valid identifier: '{}'", prog.name);
    }

    // Must have at least one action
    if prog.actions.is_empty() {
        bail!("Program must have at least one action");
    }

    // Validate each action
    for action in &prog.actions {
        validate_action(action)?;
    }

    Ok(())
}

fn validate_action(action: &Action) -> Result<()> {
    match action {
        Action::Print(msg) => {
            // Can be empty (valid to print empty string)
            // No additional validation needed for now
            Ok(())
        }
    }
}

fn validate_function(func: &FunctionTemplate) -> Result<()> {
    // Name validation
    if func.name.is_empty() {
        bail!("Function name cannot be empty");
    }

    if !func.name.chars().all(|c| c.is_alphanumeric() || c == '_') {
        bail!("Function name must be a valid identifier: '{}'", func.name);
    }

    // Must have implementation
    if func.implementation.is_empty() {
        bail!("Function must have at least one statement in implementation");
    }

    // Validate parameter names are unique
    let mut param_names = std::collections::HashSet::new();
    for param in &func.inputs {
        if !param_names.insert(&param.name) {
            bail!("Duplicate parameter name: '{}'", param.name);
        }
    }

    // Validate statements
    for stmt in &func.implementation {
        validate_statement(stmt)?;
    }

    Ok(())
}

fn validate_statement(stmt: &Statement) -> Result<()> {
    match stmt {
        Statement::Return { value } => validate_expression(value),
    }
}

fn validate_expression(expr: &Expression) -> Result<()> {
    match expr {
        Expression::Literal(_) => Ok(()),
        Expression::Variable(_) => Ok(()),
        Expression::BinaryOp { left, right, .. } => {
            validate_expression(left)?;
            validate_expression(right)?;
            Ok(())
        }
    }
}
