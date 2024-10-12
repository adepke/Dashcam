#!/usr/bin/python3

import argparse
from enum import Enum
import time
import socket
import sys
import threading
import RPi.GPIO as gpio

class StatusColors(Enum):
    OFF = 0
    RED = 1
    GREEN = 2
    BLUE = 3
    YELLOW = 4
    WHITE = 5

class DashcamState(Enum):
    DEAD = 0
    ERROR = 1
    STARTING = 2
    RECORDING = 3
    FALLING_BEHIND = 4
    CONVERTING = 5
    UPLOADING = 6

def setLight(parsedArgs, color):
    if color == StatusColors.OFF:
        gpio.output(parsedArgs.gpio_red, gpio.HIGH)
        gpio.output(parsedArgs.gpio_green, gpio.HIGH)
        gpio.output(parsedArgs.gpio_blue, gpio.HIGH)
    elif color == StatusColors.RED:
        gpio.output(parsedArgs.gpio_red, gpio.LOW)
        gpio.output(parsedArgs.gpio_green, gpio.HIGH)
        gpio.output(parsedArgs.gpio_blue, gpio.HIGH)
    elif color == StatusColors.GREEN:
        gpio.output(parsedArgs.gpio_red, gpio.HIGH)
        gpio.output(parsedArgs.gpio_green, gpio.LOW)
        gpio.output(parsedArgs.gpio_blue, gpio.HIGH)
    elif color == StatusColors.BLUE:
        gpio.output(parsedArgs.gpio_red, gpio.HIGH)
        gpio.output(parsedArgs.gpio_green, gpio.HIGH)
        gpio.output(parsedArgs.gpio_blue, gpio.LOW)
    elif color == StatusColors.YELLOW:
        gpio.output(parsedArgs.gpio_red, gpio.LOW)
        gpio.output(parsedArgs.gpio_green, gpio.LOW)
        gpio.output(parsedArgs.gpio_blue, gpio.HIGH)
    elif color == StatusColors.WHITE:
        gpio.output(parsedArgs.gpio_red, gpio.LOW)
        gpio.output(parsedArgs.gpio_green, gpio.LOW)
        gpio.output(parsedArgs.gpio_blue, gpio.LOW)
    else:
        print(f"Unknown color '{color}'", file=sys.stderr)

def watchdogRunner(queue, cv):
    watchdogPort = 5505

    listener = socket.socket(family=socket.AF_INET, proto=socket.IPPROTO_TCP)
    listener.bind(("localhost", watchdogPort))
    listener.listen()

    # Repeat this loop until the OS shuts down.
    while True:
        print("Listening for incoming connection...")
        client, _ = listener.accept()
        print("Connected!")

        try:
            while True:
                buffer = client.recv(1024).decode()
                messages = buffer.split(sep="\n")

                if len(messages) == 0:
                    continue

                with cv:
                    for message in messages:
                        queue.append(DashcamState(message))

                    cv.notify_all()

        except BaseException as exception:
            print(f"Exception thrown: {exception}\nRebooting listener...", file=sys.stderr)

            with cv:
                queue.append(DashcamState.DEAD)
                cv.notify_all()

def processMessage(parsedArgs, message):
    print(f"Setting status to {message}")
    # TODO: Implement light animation instead of fixed colors.

    if message == DashcamState.DEAD:
        setLight(parsedArgs, StatusColors.RED)
    elif message == DashcamState.ERROR:
        setLight(parsedArgs, StatusColors.RED)
    elif message == DashcamState.STARTING:
        setLight(parsedArgs, StatusColors.BLUE)
    elif message == DashcamState.RECORDING:
        setLight(parsedArgs, StatusColors.OFF)
    elif message == DashcamState.FALLING_BEHIND:
        setLight(parsedArgs, StatusColors.YELLOW)
    elif message == DashcamState.CONVERTING:
        setLight(parsedArgs, StatusColors.GREEN)
    elif message == DashcamState.UPLOADING:
        setLight(parsedArgs, StatusColors.BLUE)
    else:
        print(f"Unknown message '{message}'", file=sys.stderr)

def main():
    parser = argparse.ArgumentParser(description="Watchdog service to monitor the dashcam and report status to GPIO")
    parser.add_argument("-r", "--gpio-red", type=int, default=7, required=False)
    parser.add_argument("-g", "--gpio-green", type=int, default=9, required=False)
    parser.add_argument("-b", "--gpio-blue", type=int, default=15, required=False)
    parsedArgs = parser.parse_args()

    gpio.setmode(gpio.BCM)
    gpio.setup(parsedArgs.gpio_red, gpio.OUT)
    gpio.setup(parsedArgs.gpio_green, gpio.OUT)
    gpio.setup(parsedArgs.gpio_blue, gpio.OUT)

    setLight(parsedArgs, StatusColors.RED)

    messageQueue = list()
    queueCondition = threading.Condition()

    runnerThread = threading.Thread(target=watchdogRunner, name="Watchdog runner", args=(messageQueue, queueCondition))
    runnerThread.start()

    # Repeat this loop until the OS shuts down.
    while True:
        messages = list()

        with queueCondition:
            while len(messageQueue) == 0:
                queueCondition.wait()

            messages = messageQueue.copy()
            messageQueue.clear()

        # Currently only use the latest message, in the future this could be changed to account for all messages with a small delay.
        processMessage(parsedArgs, messages[-1])

if __name__ == "__main__":
    exit(code=main())
