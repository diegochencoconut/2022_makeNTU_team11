const mongoose = require('mongoose');

const SymptomSchema = mongoose.Schema({
    type:
    {
        type: String,
        default: "Evaluation",
    },
    room:
    {
        type: Number,
        required: true,
    },
    symptom:
    {
        type: Boolean,
        required: true,
    },
    temperature:
    {
        type: Number,
        required: true,

    },
    date:
    {
        type: Date,
        default: Date.now,
    },

})

module.exports = mongoose.model('Symptom', SymptomSchema);