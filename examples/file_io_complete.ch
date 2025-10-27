// Ejemplo: File I/O Completo
// Demuestra lectura y escritura de archivos

fn write_example_file() -> i32 {
    println("1. Escribiendo archivo de ejemplo...");

    // Flags: O_WRONLY | O_CREAT | O_TRUNC
    // 1 (WRONLY) | 64 (CREAT) | 512 (TRUNC) = 577
    let fd = open("data.txt", 577);

    if (fd < 0) {
        println("   ❌ Error: No se pudo crear el archivo");
        return -1;
    }

    // Escribir múltiples líneas
    let line1 = "Primera linea de texto";
    let line2 = "Segunda linea de texto";
    let line3 = "Tercera linea con numero: 42";
    let newline = "
";

    write(fd, line1, 22);
    write(fd, newline, 1);
    write(fd, line2, 22);
    write(fd, newline, 1);
    write(fd, line3, 29);
    write(fd, newline, 1);

    close(fd);
    println("   ✅ Archivo escrito correctamente");
    return 0;
}

fn read_entire_file() -> i32 {
    println("");
    println("2. Leyendo archivo completo...");

    // Flag: O_RDONLY = 0
    let fd = open("data.txt", 0);

    if (fd < 0) {
        println("   ❌ Error: No se pudo abrir el archivo");
        return -1;
    }

    let buffer: [i8; 1024];
    let bytes_read = read(fd, buffer, 1024);

    if (bytes_read < 0) {
        println("   ❌ Error leyendo archivo");
        close(fd);
        return -1;
    }

    print("   Bytes leídos: ");
    print_int(bytes_read);
    println("");
    println("   Contenido:");
    println("   ---");

    // Imprimir cada byte como carácter
    let i = 0;
    while (i < bytes_read) {
        // Imprimir byte a stdout usando write syscall
        write(1, buffer + i, 1);
        i++;
    }

    println("   ---");

    close(fd);
    println("   ✅ Archivo leído correctamente");
    return 0;
}

fn count_lines_in_file() -> i32 {
    println("");
    println("3. Contando líneas...");

    let fd = open("data.txt", 0);

    if (fd < 0) {
        println("   ❌ Error abriendo archivo");
        return -1;
    }

    let buffer: [i8; 1024];
    let bytes_read = read(fd, buffer, 1024);

    if (bytes_read < 0) {
        close(fd);
        return -1;
    }

    // Contar newlines
    let line_count = 0;
    let i = 0;
    while (i < bytes_read) {
        if (buffer[i] == 10) {  // '\n' = ASCII 10
            line_count++;
        }
        i++;
    }

    print("   Número de líneas: ");
    print_int(line_count);
    println("");

    close(fd);
    return 0;
}

fn append_to_file() -> i32 {
    println("");
    println("4. Añadiendo al archivo...");

    // Flags: O_WRONLY | O_APPEND
    // 1 (WRONLY) | 1024 (APPEND) = 1025
    let fd = open("data.txt", 1025);

    if (fd < 0) {
        println("   ❌ Error abriendo archivo para append");
        return -1;
    }

    let new_line = "Cuarta linea (añadida)";
    let newline = "
";

    write(fd, new_line, 22);
    write(fd, newline, 1);

    close(fd);
    println("   ✅ Línea añadida");
    return 0;
}

fn copy_file() -> i32 {
    println("");
    println("5. Copiando archivo...");

    // Abrir archivo origen
    let fd_src = open("data.txt", 0);
    if (fd_src < 0) {
        println("   ❌ Error abriendo archivo origen");
        return -1;
    }

    // Crear archivo destino
    let fd_dst = open("data_copy.txt", 577);
    if (fd_dst < 0) {
        println("   ❌ Error creando archivo destino");
        close(fd_src);
        return -1;
    }

    // Leer y escribir
    let buffer: [i8; 1024];
    let bytes_read = read(fd_src, buffer, 1024);

    if (bytes_read > 0) {
        write(fd_dst, buffer, bytes_read);
    }

    close(fd_src);
    close(fd_dst);

    print("   ✅ Archivo copiado (");
    print_int(bytes_read);
    println(" bytes)");

    return 0;
}

fn main() -> i32 {
    println("=== File I/O Completo en Chronos ===");
    println("");

    // 1. Escribir archivo
    if (write_example_file() < 0) {
        return 1;
    }

    // 2. Leer archivo
    if (read_entire_file() < 0) {
        return 1;
    }

    // 3. Contar líneas
    if (count_lines_in_file() < 0) {
        return 1;
    }

    // 4. Añadir al archivo
    if (append_to_file() < 0) {
        return 1;
    }

    // 5. Verificar append
    read_entire_file();

    // 6. Copiar archivo
    if (copy_file() < 0) {
        return 1;
    }

    println("");
    println("✅ Todas las operaciones de File I/O completadas");
    println("");
    println("Archivos creados:");
    println("  - data.txt");
    println("  - data_copy.txt");

    return 0;
}
