package com.example.whitecaneguardian

fun main() {
    val expected = listOf("포고핀 분리상태", "계단 감지", "STAIR_CLEARED", "FALL_DETECTED", "REG,OK,01020304,NEW,ALICE")
    val bytes = (expected.joinToString("\n") + "\n").toByteArray(Charsets.UTF_8)
    for (chunkSize in 1..20) {
        val parser = NavcaneLines()
        val actual = mutableListOf<String>()
        var offset = 0
        while (offset < bytes.size) {
            val end = (offset + chunkSize).coerceAtMost(bytes.size)
            actual += parser.push(bytes.copyOfRange(offset, end)); offset = end
        }
        check(actual == expected) { "UTF8 framing failed at $chunkSize bytes" }
    }
    val parser = NavcaneLines()
    check(parser.push("PARTIAL".toByteArray()).isEmpty())
    parser.clear()
    check(parser.push("ACK,CFG\r\nPONG\n".toByteArray()) == listOf("ACK,CFG", "PONG"))
    check(parser.push(("X".repeat(97) + "\nFALL_DETECTED\n").toByteArray()) == listOf("FALL_DETECTED"))
    check(parser.droppedLines == 1)
    check(parser.push("BAD\u0000LINE\nREG,TIMEOUT\n".toByteArray()) == listOf("REG,TIMEOUT"))
    check(parser.droppedLines == 2)
    check(RfidConfig.NAME_REGEX.matches("USER123"))
    check(!RfidConfig.NAME_REGEX.matches("A B"))
    check(RfidConfig.OBSTACLE_MIN_CM == 20 && RfidConfig.OBSTACLE_MAX_CM == 200)
    check(RfidConfig.HEIGHT_CODE_MIN == 14 && RfidConfig.HEIGHT_CODE_MAX == 19)
    check(RfidConfig.REG_WAIT_SEC == 15 && RfidConfig.ACK_TIMEOUT_MS == 5000L)
    println("Android JVM: UTF8 chunks 1..20, multiple lines, reset, overflow, NUL and shared RFID limits PASS")
}
