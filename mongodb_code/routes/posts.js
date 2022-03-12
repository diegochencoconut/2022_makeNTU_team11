const express = require('express');
const router = express.Router();
const Post = require('../models/Post');
const bodyParser = require('body-parser')
const jsonParser = bodyParser.json()

router.get('/', async (req, res) => {
    try
    {
        const posts = await Post.find();
        res.json(posts);
    }
    catch(err)
    {
        res.json({message:err});
    }
});

router.get('/:postId', async (req, res) => {
    try
    {   
        const post = await Post.findById(req.params.postId);
        res.json(post);
    }
    catch(err)
    {
        res.json({message:err});
    }
});

router.post('/', jsonParser, async (req, res) => {
    const post = new Post({
        title: req.body.title,
        description: req.body.description,
    });
    try
    {
        const savedPost = await post.save();
        console.log(savedPost);
        res.json(savedPost);
    }
    catch(err)
    {
        res.json({message: err})
    }
});

router.patch('/:postId', jsonParser, async(req, res)=>{
    try
    {
        const updatedPost = await Post.updateOne(
            { _id: req.params.postId },
            {$set: {title: req.body.title}}
        );
        res.json(updatedPost);
    }
    catch(err)
    {
        res.json({message: err})
    }
})

module.exports = router;