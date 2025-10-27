import 'package:flutter/material.dart';
import 'package:litealert/pages/contactConfirmedView.dart';
import '../services/contact_storage_service.dart';

class AddContactView extends StatefulWidget {
  const AddContactView({Key? key}) : super(key: key);

  @override
  State<AddContactView> createState() => _AddContactViewState();
}

class _AddContactViewState extends State<AddContactView> {
  final _formKey = GlobalKey<FormState>();
  final _nameController = TextEditingController();
  final _phoneController = TextEditingController();
  String _selectedRelationship = 'Family';
  final ContactStorageService _storageService = ContactStorageService();

  final List<String> _relationships = ['Family', 'Friend', 'Colleague', 'Other'];

  @override
  void dispose() {
    _nameController.dispose();
    _phoneController.dispose();
    super.dispose();
  }

  void _saveContact() async {
    if (_formKey.currentState!.validate()) {
      // Create contact object
      final newContact = {
        'name': _nameController.text,
        'phone': _phoneController.text,
        'relationship': _selectedRelationship,
      };

      // Save to local storage
      // await _storageService.addContact(newContact);

      if (context.mounted) {
        final result = await Navigator.push(
          context,
          MaterialPageRoute(
            builder: (context) => ContactConfirmedView(
              contactName: _nameController.text,
            ),
          ),
        );
        
        // Return to contacts page
        if (result == true && context.mounted) {
          Navigator.pop(context, newContact);
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    // ...existing code...
    return Scaffold(
      body: Column(
        children: [
          const SizedBox(height: 40),
          Container(
            width: double.infinity,
            padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 16),
            child: Row(
              children: [
                IconButton(
                  icon: const Icon(Icons.arrow_back),
                  onPressed: () {
                    Navigator.pop(context);
                  },
                ),
                const Expanded(
                  child: Text(
                    'CONTACTS',
                    style: TextStyle(
                      fontSize: 24.0,
                      fontWeight: FontWeight.bold,
                    ),
                    textAlign: TextAlign.center,
                  ),
                ),
                const SizedBox(width: 48),
              ],
            ),
          ),
          Expanded(
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(24.0),
              child: Form(
                key: _formKey,
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const SizedBox(height: 20),
                    TextFormField(
                      controller: _nameController,
                      decoration: const InputDecoration(
                        labelText: 'Name',
                        border: OutlineInputBorder(),
                        prefixIcon: Icon(Icons.person),
                      ),
                      validator: (value) {
                        if (value == null || value.isEmpty) {
                          return 'Please enter a name';
                        }
                        return null;
                      },
                    ),
                    const SizedBox(height: 20),
                    TextFormField(
                      controller: _phoneController,
                      decoration: const InputDecoration(
                        labelText: 'Phone Number',
                        border: OutlineInputBorder(),
                        prefixIcon: Icon(Icons.phone),
                      ),
                      keyboardType: TextInputType.phone,
                      validator: (value) {
                        if (value == null || value.isEmpty) {
                          return 'Please enter a phone number';
                        }
                        return null;
                      },
                    ),
                    const SizedBox(height: 20),
                    DropdownButtonFormField<String>(
                      value: _selectedRelationship,
                      decoration: const InputDecoration(
                        labelText: 'Relationship',
                        border: OutlineInputBorder(),
                        prefixIcon: Icon(Icons.people),
                      ),
                      items: _relationships.map((String relationship) {
                        return DropdownMenuItem<String>(
                          value: relationship,
                          child: Text(relationship),
                        );
                      }).toList(),
                      onChanged: (String? newValue) {
                        setState(() {
                          _selectedRelationship = newValue!;
                        });
                      },
                    ),
                    const SizedBox(height: 32),
                    ElevatedButton(
                      onPressed: _saveContact,
                      style: ElevatedButton.styleFrom(
                        backgroundColor: const Color.fromARGB(255, 200, 168, 255),
                        padding: const EdgeInsets.symmetric(vertical: 16),
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(8),
                        ),
                      ),
                      child: const Text(
                        'SAVE CONTACT',
                        style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}