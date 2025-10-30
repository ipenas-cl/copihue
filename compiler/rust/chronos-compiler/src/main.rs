use anyhow::{Context, Result};
use clap::Parser;
use std::fs;
use std::path::PathBuf;
use std::process::Command;

mod parser;
mod validator;
mod codegen;

#[derive(Parser, Debug)]
#[command(name = "chronos")]
#[command(about = "Chronos template-based deterministic language compiler", long_about = None)]
struct Args {
    /// Input template file (.chronos)
    #[arg(value_name = "FILE")]
    input: PathBuf,

    /// Output executable name
    #[arg(short, long)]
    output: Option<String>,

    /// Keep intermediate files (.s, .o)
    #[arg(short, long)]
    keep: bool,

    /// Verbose output
    #[arg(short, long)]
    verbose: bool,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Determine output name
    let output_name = args.output.unwrap_or_else(|| {
        args.input
            .file_stem()
            .unwrap()
            .to_str()
            .unwrap()
            .to_string()
    });

    if args.verbose {
        println!("Chronos Compiler v0.0.1");
        println!("========================");
        println!();
    }

    // 1. Read template
    if args.verbose {
        println!("[1/7] Reading template: {}", args.input.display());
    }
    let template_src = fs::read_to_string(&args.input)
        .with_context(|| format!("Failed to read template file: {}", args.input.display()))?;

    // 2. Parse YAML
    if args.verbose {
        println!("[2/7] Parsing template...");
    }
    let template: parser::Template = serde_yaml::from_str(&template_src)
        .context("Failed to parse template (invalid YAML)")?;

    // 3. Validate
    if args.verbose {
        println!("[3/7] Validating template...");
    }
    validator::validate_template(&template)
        .context("Template validation failed")?;

    // 4. Generate assembly
    if args.verbose {
        println!("[4/7] Generating x86-64 assembly...");
    }
    let asm_code = codegen::generate_asm(&template)
        .context("Code generation failed")?;

    // 5. Write assembly file
    let asm_path = format!("{}.s", output_name);
    fs::write(&asm_path, &asm_code)
        .with_context(|| format!("Failed to write assembly file: {}", asm_path))?;

    if args.verbose {
        println!("     Generated: {}", asm_path);
    }

    // 6. Assemble (using GNU as)
    if args.verbose {
        println!("[5/7] Assembling...");
    }
    let obj_path = format!("{}.o", output_name);
    let status = Command::new("as")
        .arg(&asm_path)
        .arg("-o")
        .arg(&obj_path)
        .status()
        .context("Failed to run assembler (is 'as' installed?)")?;

    if !status.success() {
        anyhow::bail!("Assembly failed");
    }

    // 7. Link (using ld)
    if args.verbose {
        println!("[6/7] Linking...");
    }
    let exe_path = &output_name;
    let status = Command::new("ld")
        .arg(&obj_path)
        .arg("-o")
        .arg(exe_path)
        .status()
        .context("Failed to run linker (is 'ld' installed?)")?;

    if !status.success() {
        anyhow::bail!("Linking failed");
    }

    if args.verbose {
        println!("[7/7] Cleaning up...");
    }

    // Cleanup intermediate files if not --keep
    if !args.keep {
        fs::remove_file(&asm_path).ok();
        fs::remove_file(&obj_path).ok();
    }

    println!("✓ Build successful: {}", exe_path);
    if args.verbose {
        println!();
        println!("Run with: ./{}", exe_path);
    }

    Ok(())
}
