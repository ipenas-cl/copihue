use crate::parser::*;
use anyhow::Result;
use std::sync::atomic::{AtomicUsize, Ordering};

static LABEL_COUNTER: AtomicUsize = AtomicUsize::new(0);

fn gen_label(prefix: &str) -> String {
    let id = LABEL_COUNTER.fetch_add(1, Ordering::SeqCst);
    format!(".L{}_{}", prefix, id)
}

pub fn generate_asm(template: &Template) -> Result<String> {
    let mut gen = AsmGenerator::new();
    gen.generate(template)
}

struct AsmGenerator {
    text_section: Vec<String>,
    data_section: Vec<String>,
}

impl AsmGenerator {
    fn new() -> Self {
        Self {
            text_section: Vec::new(),
            data_section: Vec::new(),
        }
    }

    fn emit_text(&mut self, line: &str) {
        self.text_section.push(line.to_string());
    }

    fn emit_data(&mut self, line: &str) {
        self.data_section.push(line.to_string());
    }

    fn generate(&mut self, template: &Template) -> Result<String> {
        match template {
            Template::Program(prog) => self.generate_program(prog),
            Template::Function(func) => self.generate_function(func),
        }
    }

    fn generate_program(&mut self, prog: &ProgramTemplate) -> Result<String> {
        // Text section
        self.emit_text(".text");
        self.emit_text(".global _start");
        self.emit_text("");
        self.emit_text("_start:");

        // Generate code for each action
        for action in &prog.actions {
            self.generate_action(action)?;
        }

        // Exit with code 0
        self.emit_text("");
        self.emit_text("    # Exit");
        self.emit_text("    movq $60, %rax      # syscall: exit");
        self.emit_text("    xorq %rdi, %rdi     # status: 0");
        self.emit_text("    syscall");

        // Combine sections
        let mut output = String::new();
        output.push_str(&self.text_section.join("\n"));
        output.push_str("\n\n");
        if !self.data_section.is_empty() {
            output.push_str(".data\n");
            output.push_str(&self.data_section.join("\n"));
            output.push_str("\n");
        }

        Ok(output)
    }

    fn generate_action(&mut self, action: &Action) -> Result<()> {
        match action {
            Action::Print(msg) => {
                let label = gen_label("str");

                // Add string to data section (with automatic newline if not present)
                let msg_with_newline = if msg.ends_with('\n') {
                    msg.clone()
                } else {
                    format!("{}\n", msg)
                };

                self.emit_data(&format!("{}:", label));
                self.emit_data(&format!("    .ascii {:?}", msg_with_newline));

                let msg_len = msg_with_newline.len();

                // Generate syscall to write
                self.emit_text("");
                self.emit_text(&format!("    # Print {:?}", msg));
                self.emit_text("    movq $1, %rax           # syscall: write");
                self.emit_text("    movq $1, %rdi           # fd: stdout");
                self.emit_text(&format!("    leaq {}(%rip), %rsi   # buffer", label));
                self.emit_text(&format!("    movq ${}, %rdx          # count", msg_len));
                self.emit_text("    syscall");
            }
        }
        Ok(())
    }

    fn generate_function(&mut self, func: &FunctionTemplate) -> Result<String> {
        // Function header
        self.emit_text(".text");
        self.emit_text(&format!(".global {}", func.name));
        self.emit_text(&format!(".type {}, @function", func.name));
        self.emit_text("");
        self.emit_text(&format!("{}:", func.name));

        // Prologue
        self.emit_text("    pushq %rbp");
        self.emit_text("    movq %rsp, %rbp");
        self.emit_text("");

        // Function body
        for stmt in &func.implementation {
            self.generate_statement(stmt)?;
        }

        // Epilogue
        self.emit_text("");
        self.emit_text("    popq %rbp");
        self.emit_text("    ret");

        // Combine sections
        let mut output = String::new();
        output.push_str(&self.text_section.join("\n"));
        output.push_str("\n");

        Ok(output)
    }

    fn generate_statement(&mut self, stmt: &Statement) -> Result<()> {
        match stmt {
            Statement::Return { value } => {
                self.emit_text("    # Return statement");
                self.generate_expression(value, "%rax")?;
                // Result already in %rax
            }
        }
        Ok(())
    }

    fn generate_expression(&mut self, expr: &Expression, dest_reg: &str) -> Result<()> {
        match expr {
            Expression::Literal(lit) => match lit {
                Literal::Integer(val) => {
                    self.emit_text(&format!("    movq ${}, {}", val, dest_reg));
                }
                Literal::String(_) => {
                    // TODO: implement string literals
                    anyhow::bail!("String literals in expressions not yet supported");
                }
            },
            Expression::Variable(name) => {
                // TODO: implement variable lookup
                anyhow::bail!("Variables not yet supported: {}", name);
            }
            Expression::BinaryOp { op, left, right } => {
                // Generate left operand into %rax
                self.generate_expression(left, "%rax")?;
                // Push left result
                self.emit_text("    pushq %rax");
                // Generate right operand into %rbx
                self.generate_expression(right, "%rbx")?;
                // Pop left into %rax
                self.emit_text("    popq %rax");

                // Perform operation
                match op {
                    BinaryOperator::Add => {
                        self.emit_text("    addq %rbx, %rax");
                    }
                    BinaryOperator::Sub => {
                        self.emit_text("    subq %rbx, %rax");
                    }
                    BinaryOperator::Mul => {
                        self.emit_text("    imulq %rbx, %rax");
                    }
                    BinaryOperator::Div => {
                        self.emit_text("    cqto             # sign extend rax to rdx:rax");
                        self.emit_text("    idivq %rbx       # rax = rdx:rax / rbx");
                    }
                }

                // Move result to destination if not %rax
                if dest_reg != "%rax" {
                    self.emit_text(&format!("    movq %rax, {}", dest_reg));
                }
            }
        }
        Ok(())
    }
}
