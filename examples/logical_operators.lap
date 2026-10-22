// Ejemplo: Operadores Lógicos
// Demuestra &&, ||, y ! (AND, OR, NOT)

fn is_valid_age(age: i32) -> i32 {
    // AND: Ambas condiciones deben ser verdaderas
    if (age >= 18 && age <= 65) {
        return 1;  // Edad válida para trabajar
    }
    return 0;
}

fn is_weekend(day: i32) -> i32 {
    // OR: Al menos una condición debe ser verdadera
    if (day == 6 || day == 7) {
        return 1;  // Sábado o domingo
    }
    return 0;
}

fn is_working_day(day: i32) -> i32 {
    // NOT: Negación lógica
    if (!is_weekend(day)) {
        return 1;  // Es día laboral
    }
    return 0;
}

fn check_access(age: i32, is_member: i32, is_vip: i32) -> i32 {
    // Operadores combinados
    // Acceso si: (edad válida Y miembro) O es VIP
    if ((age >= 18 && is_member == 1) || is_vip == 1) {
        return 1;  // Acceso permitido
    }
    return 0;
}

fn main() -> i32 {
    println("=== Demostración de Operadores Lógicos ===");
    println("");

    // Test 1: Operador AND (&&)
    println("Test 1: Operador AND (&&)");
    print("  Edad 25 es válida? ");
    if (is_valid_age(25) == 1) {
        println("Sí");
    } else {
        println("No");
    }

    print("  Edad 70 es válida? ");
    if (is_valid_age(70) == 1) {
        println("Sí");
    } else {
        println("No");
    }
    println("");

    // Test 2: Operador OR (||)
    println("Test 2: Operador OR (||)");
    print("  Día 6 es fin de semana? ");
    if (is_weekend(6) == 1) {
        println("Sí");
    } else {
        println("No");
    }

    print("  Día 3 es fin de semana? ");
    if (is_weekend(3) == 1) {
        println("Sí");
    } else {
        println("No");
    }
    println("");

    // Test 3: Operador NOT (!)
    println("Test 3: Operador NOT (!)");
    print("  Día 2 es laboral? ");
    if (is_working_day(2) == 1) {
        println("Sí");
    } else {
        println("No");
    }

    print("  Día 7 es laboral? ");
    if (is_working_day(7) == 1) {
        println("Sí");
    } else {
        println("No");
    }
    println("");

    // Test 4: Operadores combinados
    println("Test 4: Operadores Combinados");

    print("  Edad=25, Miembro=1, VIP=0: ");
    if (check_access(25, 1, 0) == 1) {
        println("Acceso OK");
    } else {
        println("Acceso Denegado");
    }

    print("  Edad=16, Miembro=1, VIP=0: ");
    if (check_access(16, 1, 0) == 1) {
        println("Acceso OK");
    } else {
        println("Acceso Denegado");
    }

    print("  Edad=16, Miembro=0, VIP=1: ");
    if (check_access(16, 0, 1) == 1) {
        println("Acceso OK");
    } else {
        println("Acceso Denegado");
    }

    println("");
    println("✅ Tests completados");

    return 0;
}
