fn main() -> i32 {
    let x = 10;
    let y = 20;

    // Test &&
    if (x > 5 && y > 15) {
        println("Ambas condiciones son verdaderas");
    }

    // Test ||
    if (x > 100 || y > 15) {
        println("Al menos una condicion es verdadera");
    }

    return 0;
}
