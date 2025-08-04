# Bookshop-Group-Scheduling-and-Resource-Management
#Abstract
This project simulates a real-life bookshop environment, managing multiple book discussion groups simultaneously. The aim is to ensure proper scheduling, resource allocation, and conflict-free operation using multithreaded programming in C. Synchronization mechanisms such as mutex locks, condition variables, and POSIX semaphores are employed to safely manage access to limited resources including rooms, chairs, and a shared book catalog. This simulation emphasizes the practical importance of mutual exclusion and inter-thread communication.
#Introduction
"Whispering Pages" is a cozy bookshop café where different book clubs meet on Saturdays to discuss popular books like Heart Bones and Kafka on the Shore. As many groups gather at the same time, it becomes hard to manage room bookings, chairs, and book availability. The shop owner needs a smart system to avoid double bookings and resource problems. This project uses a multi-threaded C program where each book club is a separate thread. The program uses locks and other tools to make sure groups don’t use the same resources at the same time, helping everything run smoothly and fairly.
#Proposed Solution
This project solves the bookshop scheduling problem using a multi-threaded C application that simulates book group activities in a shared environment. It carefully addresses resource allocation, synchronization, and thread coordination challenges using mutexes, semaphores, and condition variables.
Each book group is modeled as a thread. These threads attempt to book rooms, borrow books, and claim the necessary chairs. To avoid resource conflicts, the program enforces synchronization principles that guarantee safe access and efficient utilization of limited shared resources.
1. Data Structures
The solution defines three primary data structures: Book, Group, and Room. Each Book contains a title and information about available and total copies. A Group stores its identity, required chairs, time slot, room assignment, and selected book. The Room structure manages per-hour booking with an availability array and hour-specific mutexes.
2. Resource Initialization
The initialize_catalog() function loads the system with a fixed set of books and their quantities. Similarly, the initialize_rooms() function creates mutexes for each room and hour slot, ensuring thread-safe time-based reservations. The total number of available chairs is globally tracked.
3. Thread Synchronization Mechanisms
To prevent data inconsistencies, multiple synchronization tools are used:
●	resource_mutex ensures exclusive access to the chair count.
●	catalog_mutex secures book borrow and return operations.
●	Each room contains a room_mutex and an array of hour_mutex[] locks for fine-grained control.
●	chair_cond is a condition variable that blocks threads until enough chairs are available.
4. Room Booking
When a group thread starts, it tries to book a room using the book_time_slot() function. This function locks the room, checks for conflicts in the requested time range, and updates availability if no conflicts are found. It ensures that no two groups can occupy the same room simultaneously.
5. Chair Allocation and Book Borrowing
Once a room is booked, the group attempts to allocate the required number of chairs. If not enough chairs are available, the thread waits on chair_cond until notified. After acquiring chairs, the group selects a book from the catalog, which is displayed under a locked section to prevent simultaneous editing.
6. Meeting Simulation
Group meetings are simulated using the sleep() function, representing the time between start_hour and end_hour. During this period, the thread holds all resources it acquired, effectively blocking access by other threads to the same resources.
7. Resource Release
At the end of the meeting, the group releases all its resources. This includes:
●	Returning the borrowed book to the catalog.
●	Releasing chairs and broadcasting availability via chair_cond.
●	Releasing the room’s hourly time slots to make them available to other threads.
8. Input & Thread Handling
In the main() function, the user inputs the number of rooms and groups. For each group, chair requirements and meeting times are collected. Threads are then created and started using pthread_create(). Once all threads are active, pthread_join() is used to wait for their completion.
9. Cleanup
After all threads finish, mutexes and condition variables are destroyed to clean up resources. The final status of the book catalog is displayed to show which books were returned. This step ensures graceful termination and proper resource deallocation.

