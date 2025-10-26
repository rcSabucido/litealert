import 'package:flutter/material.dart';
import 'addContactView.dart';

class ContactsPage extends StatelessWidget {
  const ContactsPage({Key? key, required this.title}) : super(key: key);
  final String title;
  
  @override
  Widget build(BuildContext context) {
    // Sample contact data
    final contacts = [
      {'name': 'Mama TM', 'phone': '09163137061', 'relationship': 'Family'},
      {'name': 'Papa TM', 'phone': '09566784956', 'relationship': 'Friend'},
      {'name': 'JM TM', 'phone': '+09682935959', 'relationship': 'Colleague'},
    ];
    
    return Scaffold(
      body: Column(
        children: [
          const SizedBox(height: 40),
          Container(
            width: double.infinity,
            padding: const EdgeInsets.symmetric(vertical: 30),
            decoration: const BoxDecoration(
              color: Color.fromARGB(255, 200, 168, 255),
            ),
            child: Text(
              title.toUpperCase(),
              style: const TextStyle(
                color: Colors.white,
                fontSize: 24.0,
                fontWeight: FontWeight.bold,
              ),
              textAlign: TextAlign.center,
            ),
          ),
          Expanded(
            child: ListView.separated(
              padding: const EdgeInsets.all(16.0),
              itemCount: contacts.length,
              separatorBuilder: (context, index) => const Divider(),
              itemBuilder: (context, index) {
                final contact = contacts[index];
                return ListTile(
                  leading: const CircleAvatar(
                    child: Icon(Icons.person),
                  ),
                  title: Text(contact['name']!),
                  subtitle: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(contact['phone']!),
                      Text(
                        contact['relationship']!,
                        style: TextStyle(
                          color: Colors.grey[600],
                          fontSize: 12.0,
                        ),
                      ),
                    ],
                  ),
                  trailing: IconButton(
                    icon: const Icon(Icons.more_vert),
                    onPressed: () {
                      // Handle options menu
                    },
                  ),
                );
              },
            ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () async {
          final result = await Navigator.push(
            context,
            MaterialPageRoute(
              builder: (context) => const AddContactView(),
            ),
          );
          if (result != null) {
            print('New contact: $result');
          }
        },
        child: const Icon(Icons.add),
      ),
    );
  }
}